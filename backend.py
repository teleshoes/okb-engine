#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Language model for prediction engine """

import os
import cfslm, cdb


# python plugins detailed logging
if os.getenv("NGRAM_DEBUG", None):
    from tools.wrapper import LogWrapper
    cdb = LogWrapper("cdb", cdb)
    cfslm = LogWrapper("cfslm", cfslm)


class FslmCdbBackend:
    """ this class implements read/write access to prediction database

        Stock ngram are stored in a separate optimized storage
        Other data (dictionary, cluster information and learnt n-grams) are
        stored in a read/write flat file
        """

    TAGS = { '#TOTAL': (-2, -4),
             '#NA': (-1, -1),
             '#START': (-3, -3),
             '#CTOTAL': (-4, -4) }


    def __init__(self, dbfile):
        self.loaded = False
        self.dirty = False
        self.readonly = False  # for testing

        self.dbfile = dbfile
        fslm_file = dbfile
        if fslm_file[-3:] == '.db': fslm_file = fslm_file[:-3]
        fslm_file += '.ng'
        self.fslm_file = fslm_file

        self.params = dict()

    def _load(self):
        if self.loaded: return

        # load stock ngrams
        cfslm.load(self.fslm_file)

        # load read/write storage
        cdb.load(self.dbfile)

        self.loaded = True
        self.dirty = False

    def get_keys(self):
        self._load()
        return cdb.get_keys()

    def save(self):
        if self.dirty and self.loaded: cdb.save()
        self.dirty = False

    def clear(self):
        if self.dirty: self.save()
        cdb.clear()
        cfslm.clear()
        self.loaded = False

    def refresh(self):
        self._load()

    def set_param(self, key, value):
        if self.readonly: return
        cdb.set_string("param_%s" % key, str(value))
        self.params[key] = value
        self.dirty = True

    def get_param(self, key, default_value = None, cast = None):
        self._load()
        if key in self.params: return self.params[key]
        value = cdb.get_string("param_%s" % key)
        if value is None: value = default_value
        if cast: value = cast(value)
        self.params[key] = value
        return value


    def get_gram(self, ids):
        ng = list(reversed(ids))
        while ng[0] == -1: ng.pop(0)
        stock_count = max(0, cfslm.search(ng))

        user_count = user_replace = last_time = 0
        user_info = cdb.get_gram(":".join([ str(x) for x in ids ]))
        if user_info:
            (user_count, user_replace, last_time) = user_info

        if not stock_count and not user_count: return None
        return (stock_count, user_count, user_replace, last_time)


    def set_gram(self, ids, gram):
        if self.readonly: return
        self.dirty = True

        (stock_count_not_used, user_count, user_replace, last_time) = gram  # obviously stock_count is ignored
        cdb.set_gram(":".join([ str(x) for x in ids ]), float(user_count), float(user_replace), int(last_time))


    def add_word(self, word):
        if self.readonly: return
        self._load()
        self.dirty = True

        lword = word.lower()
        words = cdb.get_words(lword)
        if not words: words = dict()

        if word in words: return words[word][0]

        wid_str = cdb.get_string("param_wordid")
        wid = int(wid_str) if wid_str else 1000000

        wid += 1
        cdb.set_string("param_wordid", str(wid))

        words[word] = (wid, 0)  # new words do not belong to a cluster
        cdb.set_words(lword, words)

        self.dirty = True

        return wid


    def get_words(self, word):
        """ returns multiple solutions with all possible capitalization variations
            return value is a dict: word => (word_id, cluster_id) """

        self._load()
        result = dict()
        if word in FslmCdbBackend.TAGS:
            result[word] = FslmCdbBackend.TAGS[word]
        else:
            result = cdb.get_words(word.lower())

        return result


    def get_cluster_by_id(self, id):
        # used only for testing. no caching
        return cdb.get_string("cluster-%d" % id)

    def get_word_by_id(self, id):
        # used only for testing. no caching
        return cdb.get_word_by_id(int(id))

    def purge(self, min_count, min_day):
        if self.readonly: return
        self._load()
        self.dirty = True
        cdb.purge_grams(float(min_count), int(min_day))

    def close(self):
        self.clear()
