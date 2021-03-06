#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Language model storage for prediction engine """

import os
import cfslm, cdb
import re
import shutil, tempfile, atexit

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
             '#CTOTAL': (-4, -4),
             '#NOCLUSTER': (-5, -5)}


    def __init__(self, dbfile, readonly = False):
        self.loaded = False
        self.dirty = False
        self.readonly = readonly
        fslm_file = dbfile
        if fslm_file[-3:] == '.db': fslm_file = fslm_file[:-3]
        fslm_file += '.ng'

        self.tmpdir = None
        if readonly:
            # readonly mode is only for testing, not for use on device
            self.tmpdir = tempfile.mkdtemp(prefix = "okboard-backend")
            new_dbfile = os.path.join(self.tmpdir, os.path.basename(dbfile))
            new_fslm_file = os.path.join(self.tmpdir, os.path.basename(fslm_file))
            shutil.copyfile(dbfile, new_dbfile)
            shutil.copyfile(fslm_file, new_fslm_file)
            dbfile, fslm_file = new_dbfile, new_fslm_file
            atexit.register(self.close)

        self.dbfile = os.path.realpath(dbfile)
        self.fslm_file = os.path.realpath(fslm_file)

        self.params = dict()

        self.id2tag = dict()
        for tag, value in FslmCdbBackend.TAGS.items(): self.id2tag[value[0]] = tag


    def _load(self):
        if self.loaded: return

        # load stock ngrams
        cfslm.load(self.fslm_file)

        # load read/write storage
        cdb.load(self.dbfile)

        self.loaded = True
        self.dirty = False

    def get_keys(self):
        """ get all keys in the database (n-grams, words, clusters, parameters all mixed-up) """
        self._load()
        return cdb.get_keys()

    def save(self):
        """ save database to disk storage """
        if self.dirty and self.loaded and not self.readonly: cdb.save()
        self.dirty = False

    def clear(self):
        """ remove database from memory (do not clear disk storage) """
        if self.dirty: self.save()
        cdb.clear()
        cfslm.clear()
        self.loaded = False

    def set_param(self, key, value):
        """ sets a parameter """
        cdb.set_string("param_%s" % key, str(value))
        self.params[key] = value
        self.dirty = True

    def get_param(self, key, default_value = None, cast = None):
        """ get a parameter value """
        self._load()
        if key in self.params: return self.params[key]
        value = cdb.get_string("param_%s" % key)
        if value is None: value = default_value
        if cast: value = cast(value)
        self.params[key] = value
        return value


    def get_gram(self, ids):
        """ get a n-gram metadata (returns a tuple with stock count, user count, user replace cout, and last modification time """
        ng = list(reversed(ids))
        while ng and ng[0] == -1: ng.pop(0)
        if not ng: return None

        stock_count = max(0, cfslm.search(ng))

        user_count = user_replace = last_time = 0
        user_info = cdb.get_gram(":".join([ str(x) for x in ids ]))
        if user_info:
            (user_count, user_replace, last_time) = user_info

        if not stock_count and not user_count: return None
        return (stock_count, user_count, user_replace, last_time)


    def set_gram(self, ids, gram):
        """ sets a n-gram metadata (same format as returned by get_ngram function) """
        self.dirty = True

        (stock_count_not_used, user_count, user_replace, last_time) = gram  # obviously stock_count is ignored
        cdb.set_gram(":".join([ str(x) for x in ids ]), float(user_count), float(user_replace), int(last_time))


    def add_word(self, word):
        """ add a new user learned word """
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

    def get_word(self, word):
        """ returns word information (word_id, cluster_id) """
        words = self.get_words(word.lower())
        if not words or word not in words: return None
        return words[word]

    def get_cluster_by_id(self, id):
        """ get cluster information for a given cluster id """
        return cdb.get_string("cluster-%d" % id)

    def get_word_by_id(self, id):
        """ get word """
        return cdb.get_word_by_id(int(id))

    def purge(self, min_count, min_day):
        """ purge old entries """
        if self.readonly: return
        self._load()
        self.dirty = True
        cdb.purge_grams(float(min_count), int(min_day))

    def close(self):
        """ close database """
        if self.tmpdir:
            try:
                os.unlink(self.dbfile)
                os.unlink(self.fslm_file)
                os.rmdir(self.tmpdir)
            except: pass
            self.tmpdir = None
        self.clear()

    def factory_reset(self):
        """ remove all user learned information (n-grams and new words) """
        self._load()

        update = False
        lst = cdb.get_keys()
        for key in lst:
            if key.startswith("param_"):
                continue  # keep parameters
            elif key.startswith("cluster-"):
                continue  # keep clusters
            elif re.match(r'^[0-9\-]+:[0-9\-]+:[0-9\-]+$', key):
                cdb.rm(key)  # remove all learned n-grams
                update = True
            elif re.match(r'^[\w\-\']+$', key):
                words = cdb.get_words(key)
                for word, info in list(words.items()):
                    if info[0] >= 1000000:  # stock words have a lower ID
                        update = True
                        del words[word]  # remove user learned words
                if words:
                    cdb.set_words(key, words)
                else:
                    cdb.rm(key)

        if update and not self.readonly: cdb.save()

    def get_all_words(self):
        """ get all known words (no efficient, only for tests) """
        self._load()

        result = dict()

        lst = cdb.get_keys()
        for key in lst:
            if key.startswith("param_"): continue
            if key.startswith("cluster-"): continue
            if re.match(r'^[0-9\-]+:[0-9\-]+:[0-9\-]+$', key): continue
            if not re.match(r'^[\w\-\']+$', key): continue

            words = cdb.get_words(key)
            for word, info in list(words.items()):
                result[word] = info

        return result


    def _id2word(self, id):
        if id in self.id2tag: return self.id2tag[id]
        return self.get_word_by_id(id)  # None if not found

    def _word2id(self, word):
        if word in FslmCdbBackend.TAGS: return FslmCdbBackend.TAGS[word][0]
        wid = self.get_word(word)
        if wid: return wid[0]
        return self.add_word(word)

    def file_export(self, f):
        """ dump all user data to a text file (used for version upgrade) """
        self._load()


        lst = cdb.get_keys()
        for key in lst:
            if key.find(":") == -1: continue
            gram = [ self._id2word(int(x)) for x in key.split(":") ]
            if None in gram or len(gram) != 3: continue  # unknown word, should not happen
            gramtxt = ':'.join(gram)

            (user_count, user_replace, last_time) = cdb.get_gram(key)
            f.write(';'.join([ str(x) for x in [ gramtxt, user_count, user_replace, last_time ] ]) + "\n")

    def file_import(self, f):
        """ reads all user data from a text file (used for version upgrade) """
        self._load()

        for li in f.readlines():
            cols = li.strip().split(';')
            if len(cols) != 4: continue

            (gramtxt, user_count, user_replace, last_time) = (cols[0], float(cols[1]), float(cols[2]), float(cols[3]))
            gram = [ self._word2id(x) for x in gramtxt.split(':') ]

            self.set_gram(gram, [ 0, user_count, user_replace, last_time ])

        self.save()
