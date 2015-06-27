#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Simple prediction algorithm based on N-grams and word clustering """

import os
import time
import re
import math
import cfslm, cdb
import unicodedata
import pickle

# uncomment this 3 line to activate python plugins detailed logging
# from tools.wrapper import LogWrapper
# cdb = LogWrapper("cdb", cdb)
# cfslm = LogWrapper("cfslm", cfslm)

class Wordinfo:
    def __init__(self, id, cluster_id, real_word = None):
        self.id = id
        self.cluster_id = cluster_id
        self.real_word = real_word

class FslmCdbBackend:
    """ this class implements read/write access to prediction database

        Stock ngram are stored in a separate optimized storage
        Other data (dictionary, cluster information and learnt n-grams) are
        stored in a read/write flat file
        """

    def __init__(self, dbfile):
        self.loaded = False
        self.dirty = False
        self.readonly = False  # for testing

        self.dbfile = dbfile
        fslm_file = dbfile
        if fslm_file[-3:] == '.db': fslm_file = fslm_file[:-3]
        fslm_file += '.ng'
        self.fslm_file = fslm_file

        self.clear_stats()

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
        if key in self.params: return self.params[key]
        value = cdb.get_string("param_%s" % key)
        if value is None: value = default_value
        if cast: value = cast(value)
        self.params[key] = value
        return value

    def clear_stats(self):
        self.timer =  self.count = 0


    def get_grams(self, ids_list):
        result = dict()
        _start = time.time()

        for ids in ids_list:
            (id1, id2, id3) = ids.split(':')
            ng = [ int(x) for x in [id3, id2, id1] ]
            while ng[0] == -1: ng.pop(0)
            stock_count = max(0, cfslm.search(ng))

            user_count = user_replace = last_time = 0
            user_info = cdb.get_gram(ids)
            if user_info:
                (user_count, user_replace, last_time) = user_info

            if not stock_count and not user_count: continue
            result[ids] = (stock_count, user_count, user_replace, last_time)

        self.timer += time.time() - _start
        self.count += 1

        return result

    def set_grams(self, grams):
        if self.readonly: return
        if not grams: return
        self.dirty = True

        _start = time.time()

        for ids, values in grams.items():
            (stock_count_not_used, user_count, user_replace, last_time) = values  # obviously stock_count is ignored

            cdb.set_gram(ids, float(user_count), float(user_replace), int(last_time))

        self.timer += time.time() - _start
        self.count += 1


    def add_word(self, word):
        if self.readonly: return
        self._load()
        self.dirty = True

        _start = time.time()

        lword = word.lower()
        words = cdb.get_words(lword)
        if not words: words = dict()

        if word in words: return words[word][0]

        wid_str = cdb.get_string("param_wordid")
        wid = int(wid_str) if wid_str else 1000000

        wid += 1
        cdb.set_string("param_wordid", str(wid))

        words[word] = (wid, 0)  # new words do not belong a cluster
        cdb.set_words(lword, words)

        self.dirty = True

        self.timer += time.time() - _start
        self.count += 1

        return wid

    def get_words(self, words, lower_first_words = None):
        self._load()

        if not lower_first_words: lower_first_words = set()

        in_word = dict()
        for word in words:
            if word.startswith('#'): continue
            lword = word.lower()
            if lword not in in_word: in_word[lword] = set()
            in_word[lword].add(word)

        result = dict()
        for lword in in_word.keys():
            found1 = cdb.get_words(lword)
            if not found1: continue

            lower_first = (len([ x for x in in_word[lword] if x in lower_first_words ]) > 0)

            for word in in_word[lword]:
                if lower_first and word[1:].islower() and lword in found1 and lword not in in_word[lword]:
                    # lowercase version not requested but found --> choose this one (first word only)
                    result[word] = found1[lword]
                elif word in found1:
                    # else return word with exact capitalization if found
                    result[word] = found1[word]
                elif lword not in in_word[lword] and lword in found1:
                    # fallback to lowercase word
                    result[word] = found1[lword]
                else:  # at last resort, match word with other capitalization (e.g. this is used for English "I")
                    other_words = [ w for w in found1.keys() if w not in in_word[lword] ]
                    if other_words:
                        result[word] = found1[other_words[0]]  # can't do better than random choice

        result['#TOTAL'] = (-2, -4)
        result['#NA'] = (-1, -1)
        result['#START'] = (-3, -3)
        result['#CTOTAL'] = (-4, -4)

        for k, v in result.items():
            result[k] = Wordinfo(v[0], v[1])

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


class WordScore:
    def __init__(self, score, cls, star):
        (self.score, self.cls, self.star) = (score, cls, star)
        self.final_score = -1
        self.message = "(Ignored)"

    def __str__(self):
        return (("%.2f" % self.score)
                + ((":%d" % self.cls) if self.cls else "")
                + ("*" if self.star else ""))

    @classmethod
    def copy(cls, old):
        return WordScore(old.score, old.cls, old.star)


class HistPercentile:
    VERSION = 4

    def __init__(self, percent = 0.05, size = 500, min_ratio = None):
        self.values = []
        self.high = self.low = 0
        self.percent = percent
        self.size = size
        self.min_ratio = min_ratio
        self.low = self.high = 0

    def normalize(self, value, sat = 0):
        self.values.append(value)
        if len(self.values) < 10: self.update()
        if not self.high or self.high <= self.low: return 0.5
        return min(1, max(0, (1 + 2 * sat) * (value - self.low) / (self.high - self.low) - sat))

    def update(self):
        self.values = self.values[-self.size:]
        lst = sorted(self.values)
        l = len(lst)
        if l >= 2:
            self.high = lst[min(l - 1, int(l * (1 - self.percent)))]
            self.low = lst[min(l - 1, int(l * self.percent))]
        if self.min_ratio: self.low = min(self.low, self.high * self.min_ratio)


class Predict:
    def __init__(self, tools = None, cfvalues = dict()):
        self.predict_list = [ ]
        self.tools = tools
        self.cfvalues = cfvalues
        self.db = None
        self.preedit = ""
        self.surrounding_text = ""
        self.last_surrounding_text = ""
        self.cursor_pos = -1
        self.last_words = []
        self.sentence_pos = -1
        self.last_guess = None
        self.learn_history = dict()
        self.first_word = None
        self.mock_time = None
        self.guess_history = dict()
        self.guess_history_last = [ ]
        self.coef_score_predict = [ None, None ]
        self.last_use = 0
        self.curve_learn_info_hist = [ ]
        self.corpus_size = None

        self.mod_ts = time.ctime(os.stat(__file__).st_mtime)

        self._score_hist = HistPercentile()
        self._speed_hist = HistPercentile(min_ratio = 0.5)
        self._hist_dirty = False
        self.hist_file = None

    def _mock_time(self, time):
        self.mock_time = time

    def _now(self):
        if self.mock_time: return self.mock_time
        return int(time.time())

    def cf(self, key, default_value, cast = None):
        """ "mockable" configuration """
        if self.db and self.db.get_param(key):
            ret = self.db.get_param(key)  # per language DB parameter values
        elif self.tools:
            ret = self.tools.cf(key, default_value, cast)
        elif self.cf:
            ret = self.cfvalues.get(key, default_value)
        else:
            ret = default_value

        if cast: ret = cast(ret)
        return ret

    def log(self, *args, **kwargs):
        """ log with default simple implementation """
        if self.tools:
            self.tools.log(*args, **kwargs)
        else:
            print(' '.join(map(str, args)))

    def set_dbfile(self, dbfile):
        """ select database file name to load """
        self.dbfile = dbfile
        if self.db:
            self.db.close()
        self.db = None

    def _dummy_request(self):
        # run somme dummy request to avoid huge processing time for first request
        words = [ self.db.get_word_by_id(id) for id in range(11, 30) ]
        words = [ w for w in words if w ]
        context = [ words.pop(0), words.pop(1) ]
        words = dict([ (w, WordScore(0.8, 0, False)) for w in words ])
        self._guess1(context, words)

    def refresh_db(self):
        # force DB (re-)loading for faster reads
        if self.db:
            self.log("DB refresh...")
            now = time.time()
            self.db.refresh()
            self.log("DB refresh done (%2f s)" % (time.time() - now))

    def load_db(self, force_reload = False):
        """ load database if needed """
        if self.db and not force_reload: return True

        if not os.path.isfile(self.dbfile):
            self.log("DB not found:", self.dbfile)
            return False

        self.db = FslmCdbBackend(self.dbfile)
        self.log("DB open OK:", self.dbfile)
        self.refresh_db()

        self.half_life = self.cf('learning_half_life', 30, int)
        self.coef_score_predict[0] = self.cf("predict_coef_low",   0.4, float)
        self.coef_score_predict[1] = self.cf("predict_coef_high", 0.15, float)

        self.corpus_size = self.db.get_grams(["-2:-1:-1"])["-2:-1:-1"][0]  # grand total #NA:#NA:#TOTAL
        try:
            self._dummy_request()
        except Exception as e:
            self.log("Dummy request failed (not fatal but probably wrong): " + e)

        return True

    def save_db(self):
        """ save database immediately """
        if self.db: self.db.save()

    def update_preedit(self, preedit):
        """ update own copy of preedit (from maliit) """
        self.preedit = preedit

    def _commit_learn(self, commit_all = False):
        if not self.learn_history: return

        t0 = time.time()
        self.db.clear_stats()

        current_day = int(self._now() / 86400)

        if commit_all:
            learn = self.learn_history.keys()
        else:
            now = int(time.time())
            delay = self.cf('commit_delay', 20, int)
            learn = [ key for key, item in self.learn_history.items() if item[2] < now - delay ]

        if not learn: return

        # lookup all words from DB
        word_set = set(['#TOTAL', '#NA', '#START'])
        lower_first = set([])
        for key in learn:
            (action, wordl, ts, pos) = self.learn_history[key]
            for i in range(0, len(wordl)):
                word = wordl[i]
                word_set.add(word)
                if i == pos:
                    lower_first.add(word)

        word2id = self.db.get_words(word_set, lower_first_words = lower_first)
        word2id = dict([ (w, i.id) for (w, i) in word2id.items() ])

        # list all n-grams lines to update
        na_id = word2id['#NA']
        total_id = word2id['#TOTAL']
        todo = dict()
        for key in set(learn):
            # action == True -> new occurrence / action == False -> error corrected
            (action, words, ts, pos) = self.learn_history[key]
            while words:
                wids = [ word2id.get(x, na_id) for x in words ]

                if wids[0] == na_id:  # typed word is unknown
                    new_word = words[0]
                    if len(words) == 1 or words[1] == '#START':
                        # first word, let's un-capitalize it (which may be wrong!)
                        if new_word[0].isupper() and new_word[1:].islower():
                            new_word = new_word.lower()
                    self.log("Learning new word: %s" % new_word)
                    word2id[new_word] = wids[0] = self.db.add_word(new_word)
                    if not wids[0]: continue  # read only DB: we can't learn new words

                try:  # manage unknown words
                    pos = wids.index(na_id)
                    wids = wids[0:pos]
                except:
                    pass

                while len(wids) < 3: wids.append(na_id)
                wids = [ str(x) for x in wids ]

                todo[':'.join(wids)] = action
                if action:
                    # user correction accounted only on n-gram, not on total one
                    todo[':'.join([ str(total_id) ] + wids[1:])] = action

                words.pop(-1)  # try a smaller-N-gram

            self.learn_history.pop(key, None)

        # get all n-gram lines information to cache
        grams = self.db.get_grams(todo.keys())

        # apply modifications
        for wids, action in todo.items():
            values = grams.get(wids, (0, 0, 0, 0))  # default value for creating a new line in prediction database
            (stock_count, user_count, user_replace, last_time) = values
            if not last_time:
                user_count = user_replace = last_time = 0

            elif current_day > last_time:
                # data ageing
                coef = math.exp(-0.7 * (current_day - last_time) / float(self.half_life))
                user_count *= coef
                user_replace *= coef
                last_time = current_day

            # action
            if action:
                user_count += 1
            else:
                user_replace += 1

            grams[wids] = todo[wids] = (stock_count, user_count, user_replace, current_day)
            # self.log("learn update: [%s] %.2f:%.2f:%d" % (wids, user_count, user_replace, last_time))

        # store to database
        self.db.set_grams(todo)

        t = int((time.time() - t0) * 1000)
        self.log("Commit: Lines updated:", len(todo), "elapsed:", t,
                 "db_time:", int(1000 * self.db.timer), "db_count:", self.db.count)

    def _learn(self, add, word, context, replaces = None, silent = False):
        if not self.db: return

        if re.search(r'\d', word): return  # don't learn numbers (they are matched by \w regexp)

        now = int(time.time())

        pos = -1
        try:
            pos = context.index('#START')
            context = context[0:pos + 1]
        except ValueError:
            pass  # #START not found, this is not an error

        context = context[:2]
        wordl = [ word ] + context

        if not silent:
            self.log("Learn:", "add" if add else "remove", wordl, "{replaces %s}" % replaces if replaces else "")

        wordl = wordl[0:3]
        key = '_'.join(wordl)
        if add:
            self.learn_history[key] = (True, wordl, now, pos)  # add occurence
            if replaces and replaces != word:  # ignore replacement by the same word
                key2 = '_'.join(([ replaces ] + context)[0:3])
                self.learn_history[key2] = (False, [ replaces ] + context, now, pos)  # add replacement occurence (failed prediction)
        else:
            if key in self.learn_history and not self.learn_history[key][0]:
                pass  # don't remove word replacements
            else:
                self.learn_history.pop(key, None)

    def _word2letters(self, word):
        letters = ''.join(c for c in unicodedata.normalize('NFD', word.lower()) if unicodedata.category(c) != 'Mn')
        letters = re.sub(r'[^a-z]', '', letters.lower())
        letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
        return letters

    def _get_curve_learn_info(self, word, first_word):
        """ generate information for curve matching to add a new word into user
            dictionary (or at least update statistics) """

        # we don't check in word DB and just return the word as-is (with just some basic capitalization fix)
        if first_word and word[0].isupper() and word[1:].islower():
            word = word.lower()

        # compute letter list (for curve matching plugin) - just copy-paste (ahem) from loadkb.py
        letters = self._word2letters(word)

        return (letters, word)

    def update_surrounding(self, text, pos):
        """ update own copy of surrounding text and cursor position """
        self.surrounding_text = text
        self.cursor_pos = pos

        self.last_use = time.time()

        curve_learn_info = None

        verbose = self.cf("verbose", False, bool)

        if pos == -1: return  # disable recording if not in a real text box

        def only_current_sentence(text):
            return re.sub(r'[\.\!\?]', ' #START ', text)

        list_before = [ x for x in re.split(r'[^\w\'\-\#]+', only_current_sentence(self.last_surrounding_text)) if x ]
        list_after = [ x for x in re.split(r'[^\w\'\-\#]+', only_current_sentence(self.surrounding_text)) if x ]

        if list_after == list_before: return

        if list_after and list_after[-1].lower() == 'idkfa':
            # trigger a crash (to test logging and user display)
            raise Exception("Test exception (this is a drill!)")  # @todo remove this for production code :-)

        begin = []
        end = []
        while list_before and list_after and list_before[0] == list_after[0]:
            list_before.pop(0)
            begin.append(list_after.pop(0))

        while list_before and list_after and list_before[-1] == list_after[-1]:
            list_before.pop(-1)
            end.insert(0, list_after.pop(-1))

        context = list(reversed(begin)) + ['#START']
        if not list_before and len(list_after) == 1:
            # new word
            word = list_after[0]

            # Return information for curve matching QML plugin to learn this
            # word or update statistics
            # We could add this word after the learning timeout (with a lot
            # more context & word DB checking), but it is important to add it
            # now for "wow-effect" during demos
            curve_learn_info = self._get_curve_learn_info(word, len(context) == 1)

            if not begin and not end:
                # single word sentence
                self.first_word = word  # keep first word for later
            else:
                if self.first_word and len(begin) == 1 and begin[0] == self.first_word:
                    self._learn(True, self.first_word, [ '#START' ], silent = not verbose)  # delayed learning for first word
                self._learn(True, word, context, silent = not verbose)

            # @todo if there is text following new word "unlearn" it (only n-grams with n >= 2)
            #       self._learn(False, end[0], context, silent = not verbose)

        elif len(list_before) == 1 and not list_after:
            # word removed
            word = list_before[0]
            if (begin or end) or word == self.preedit:
                # we "un-learn" words only when sentence is not empty (to distinguish a removal from a jump to another location)
                # or if the word removed has been made into a preedit
                #  (^^^ this last tests depends on the preedit change notification is sent before surrounding text one)
                self._learn(False, word, context, silent = not verbose)

                # @todo if there is text following removed word, learn [ context + following word ] (only n-grams with n >= 2)

        elif len(list_before) == 1 and len(list_after) == 1:
            # word replaced
            word1 = list_before[0]
            word2 = list_after[0]
            if len(word2) < len(word1) and word2 == word1[0:len(word2)]:
                return  # backspace into a word (don't update self.last_surrounding_text)
            else:
                self._learn(True, word2, context, replaces = word1, silent = not verbose)

        else:
            # other case are too complicated: we just ignore them at the moment
            pass

        self.last_surrounding_text = text

        if not curve_learn_info: return None
        if curve_learn_info in self.curve_learn_info_hist: return None
        self.curve_learn_info_hist = self.curve_learn_info_hist[-10:]
        self.curve_learn_info_hist.append(curve_learn_info)

        return curve_learn_info  # return words seen (this allows to learn new words in curve engine)

    def replace_word(self, old, new, context = None):
        """ inform prediction engine that a guessed word has been replaced by the user """
        # note: this can not be reliably detected with surrounding text only
        # -> case i've got a one word sentence and replace it --> and can't make the difference with moving to another location in the text

        if not context: context = self.last_words
        context = list(context)
        self._learn(True, new, context, replaces = old)

        if not self.cf('predict_auto_tune', True, bool): return

        # use replacement information to auto-tune prediction weight
        oldkey = ':'.join([ old ] + context)
        if oldkey in self.guess_history and new in self.guess_history[oldkey]["words"]:
            gh = self.guess_history[oldkey]

            gh["replaced"] = new
            newkey = ':'.join([ new ] + context)
            self.guess_history[newkey] = gh
            del self.guess_history[oldkey]

            infonew = gh["words"][new]
            infoguess = gh["words"][gh["guess"]]

            # update coefficients
            increase = decrease = False
            if infoguess.score_predict < infonew.score_predict:
                increase = True
            if infoguess.score_no_predict < infonew.score_no_predict:
                decrease = True

            if increase != decrease:
                quality_index = infoguess.quality_index

                name, sign = ("increase", 1) if increase else ("decrease", -1)

                c = 1 - quality_index
                weights = dict()
                for i in [0, 1]: (weights[i], c) = (c, 1 - c)

                for i, weight in weights.items():
                    old_value = self.coef_score_predict[i]
                    self.coef_score_predict[i] += weight * sign * self.cf("score_predict_tune_increment", 0.01, float)
                    self.coef_score_predict[i] = max(0.2, min(5, self.coef_score_predict[i]))
                    self.log("Replacement [%s -> %s] - Predict coef %s, value[%d]: %.3f -> %.3f" %
                             (old, new, name, i, old_value, self.coef_score_predict[i]))

                self.db.set_param("predict_coef_low",  self.coef_score_predict[0])
                self.db.set_param("predict_coef_high", self.coef_score_predict[1])


    def get_predict_words(self):
        """ return prediction list, in a format suitable for QML ListModel """
        return [ { "text": word } for word in self.predict_list ]

    def ping(self):
        """ hello """
        self.log("ping\n")
        return 1

    def _update_last_words(self):
        self.last_words, self.sentence_pos = self._get_last_words()

    def _get_last_words(self, count = None, include_preedit = True):
        """ internal method : compute list of last words for n-gram scoring """

        # if no context available, handle this as isolated words instead of the beginning of a new sentence (so no #START)
        if self.cursor_pos == -1: return [], -1

        st = self.surrounding_text[0:self.cursor_pos]
        if self.cursor_pos > 0 and self.cursor_pos < len(self.surrounding_text) \
           and re.match(r'[\w\'\-]+', self.surrounding_text[self.cursor_pos - 1:self.cursor_pos + 1]):
            # we are in the middle of a word, don't include it in the context
            st = re.sub(r'[\w\'\-]+$', '', st)

        text_before = st + (" " + self.preedit) if include_preedit else ""
        text_before = re.sub(r'^.*[\.\!\?]', '', text_before)

        words = reversed(re.split(r'[^\w\'\-]+', text_before))
        words = [ x for x in words if len(x) > 0 ]

        pos = len(words)
        if count:
            words = (words + [ '#START' ] * count)[0:count]
        else:
            words = words + [ '#START' ]
        return words, pos


    def _get_all_predict_scores(self, words, context, return_score_num_type = None, curve_scores = None):
        """ get n-gram DB information (use 2 SQL batch requests instead of a ton of smaller ones, for huge performance boost) """
        result = dict()
        if not words: return result

        # get previous words (surrounding text) for lookup
        words_set = set(words)
        for word in context: words_set.add(word)
        words_set.add('#START')

        # idenfity words that would need to be looked-up lowercase first (first words in sentences)
        try_lower_first = None
        if self.sentence_pos == 0:
            try_lower_first = set(words)  # first word -> all candidates
        elif self.sentence_pos == 1 and len(context) >= 1:
            try_lower_first = set([context[0]])
        elif self.sentence_pos == 2 and len(context) >= 2:
            try_lower_first = set([context[1]])

        # lookup all words from DB or cache (surrounding & candidates)
        word2id = self.db.get_words(words_set, lower_first_words = try_lower_first)

        # get id for previous words
        last_wids = []
        last_cids = []
        for word in context:
            if word not in word2id: break  # unknown word -> only use smaller n-grams
            last_wids.append(word2id[word].id)
            last_cids.append(word2id[word].cluster_id)

        # prepare scores request
        def add(word, todo, ids_list, score_name, lst, cluster = None):
            lst = [ str(x) for x in lst ]
            while len(lst) < 3: lst.append('-1')  # N/A
            num = ':'.join(lst)
            den = ':'.join([ '-2' if not cluster else '-4' ] + lst[1:])  # #TOTAL for words, #CTOTAL for clusters
            ids_list.add(num)
            ids_list.add(den)
            if word not in todo: todo[word] = dict()
            context_key = ':'.join([ score_name ] + lst[1:])
            todo[word][score_name] = (num, den, cluster, context_key)

        todo = dict()
        ids_list = set(["-2:-1:-1"])
        word2cid = dict()
        for word in words:
            result[word] = dict()
            if word not in word2id:  # unknown word -> dummy scoring
                continue
            wid, cid = word2id[word].id, word2id[word].cluster_id

            add(word, todo, ids_list, "s1", [wid])
            if cid:
                word2cid[word] = cid
                add(word, todo, ids_list, "c1", [cid], True)  # used only for P(Wi|Ci)

            if len(last_wids) >= 1:
                add(word, todo, ids_list, "s2", [wid, last_wids[0]])
                if cid and last_cids[0] != 0:
                    add(word, todo, ids_list, "c2", [cid, last_cids[0]], True)

            if len(last_wids) >= 2:
                add(word, todo, ids_list, "s3", [wid] + last_wids[0:2])
                if cid and last_cids[0] != 0 and last_cids[1] != 0:
                    add(word, todo, ids_list, "c3", [cid] + last_cids[0:2], True)

        # request
        sqlresult = self.db.get_grams(ids_list)

        # weight each context
        class ContextCount: pass

        context_count = dict()
        for word in todo:
            for score_name in todo[word]:
                num, den, cluster, ck = todo[word][score_name]
                if num not in sqlresult: continue

                stock_count = sqlresult[num][0]

                if ck not in context_count:
                    context_count[ck] = ContextCount()
                    context_count[ck].count = context_count[ck].total = 0

                context_count[ck].count += 1
                context_count[ck].total += stock_count ** 2

        for ck in context_count: context_count[ck].avg = math.sqrt(float(context_count[ck].total) / context_count[ck].count)

        # process request result & compute scores
        current_day = int(self._now() / 86400)

        (global_stock_count, global_user_count) = sqlresult["-2:-1:-1"][0:2]  # grand total #NA:#NA:#TOTAL

        # evaluate score components
        for word in todo:
            detail = dict(probas = dict(), filter = dict(), count = dict())

            if word in word2cid:
                detail["clusters"] = [ word2cid[word] ] + last_cids[0:2]

            # compute P(Wi|Ci) term for cluster n-grams
            coef_wc = None
            if "s1" in todo[word] and "c1" in todo[word]:
                numw_id = todo[word]["s1"][0]
                numc_id = todo[word]["c1"][0]
                num_w = sqlresult.get(numw_id, None)
                num_c = sqlresult.get(numc_id, None)
                if num_w and num_c and num_c[0] and num_w[0]:
                    coef_wc = 1.0 * num_w[0] / num_c[0]

            for score_name in [ "s3", "c3", "s2", "c2", "s1" ]:
                if score_name not in todo[word]: continue
                if score_name == "c1": continue  # useless

                num_id, den_id, cluster, context_key = todo[word][score_name]
                num = sqlresult.get(num_id, None)
                if not num: continue
                den = sqlresult.get(den_id, None)
                if not den: continue

                # get predict DB data
                (stock_count, user_count, user_replace, last_time) = num
                (total_stock_count, total_user_count, dummy1, dummy2) = den

                if user_replace and not total_user_count: total_user_count = 1  # hack for words that have always been replaced in this context


                detail_txt = "stock=%d/%d" % (stock_count, total_stock_count)

                # add P(Wi|Ci) term for cluster n-grams
                coef = 1.0
                if cluster:
                    # this is a "c*" score (cluster)
                    if not coef_wc: continue
                    coef = coef_wc
                    detail_txt += " coef=%.5f" % coef

                proba = stock_proba = 1.0 * coef * stock_count / total_stock_count if total_stock_count > 0 else 0

                if total_user_count and (user_count or user_replace) \
                   and global_user_count and not cluster:
                    detail_txt += (" user=[%d-%d/%d age=%d]" % (user_count, user_replace, total_user_count, current_day - last_time))

                c = detail["count"]
                if stock_count: c[score_name] = stock_count
                if user_count or user_replace:
                    c["%s-user" % score_name ] = (user_count, user_replace, total_user_count, current_day - last_time)

                if proba > 0: detail["probas"][score_name] = proba
                detail[score_name] = detail_txt

            result[word] = detail

        # evaluate score for all words
        return self._filter_final_score(result,
                                        return_score_num_type = return_score_num_type,
                                        curve_scores = curve_scores)

    def _filter_func(self, delta_proba_log):
        x = delta_proba_log

        # Old formula: y = math.pow(x, 4) / (3 + math.pow(x, 3)) / (0.2 + x / 4)

        y = (1 - math.cos(math.pi * min(x, 4) / 4)) / 2  # this one is prettier

        return y

    def _filter_final_score(self, details, return_score_num_type = None, curve_scores = None):
        flt_max = self.cf('filter_max', 1, float)  # now our scores are in the [0, 1] range and their weight is controlled by coef_score_predict parameters
        flt_coef = self.cf('filter_coef', .75, float)
        flt_min_count = self.cf('filter_min_count', 25, int)  # @todo this is highly dependent on corpus size, so it should not be a fixed parameter
        flt_curve_score_threshold = self.cf('filter_curve_score_threshold', 0.08, float)
        flt_c2s_backoff = self.cf('filter_c2s_backoff', 0.5, float)
        flt_word_max_ratio = self.cf('filter_word_max_ratio', 100, float)
        flt_user_min_count = self.cf('filter_user_min_count', 10, float)
        flt_user_rank_factor = self.cf('filter_user_rank_factor', 3.16, float)

        all_scores = [ "s3", "c3", "s2", "c2", "s1" ]

        if curve_scores: max_curve_score = max([ curve_scores.get(w, 0) for w in details ] + [ 0 ])
        max_s1 = max([ details[w]["probas"].get("s1", 0) for w in details if details[w] ] + [ 0 ])

        # -- user score: very basic stuff (on purpose) --
        learning_master_switch = self.cf('learning_enable', True, bool)
        max_user_proba = 0
        if learning_master_switch:
            for word in details:
                if not details[word]: continue
                N = flt_user_min_count
                for sc in [ "s3", "s2", "s1" ]:
                    (user_count, user_replace, total_user_count, age) = details[word]["count"].get("%s-user" % sc, (0, 0, 0, 0))
                    if user_count or user_replace: break
                    N *= flt_user_rank_factor
                    # @todo backoff or linear combination ?

                count = (user_count - user_replace)
                age_coef = age / float(self.half_life)

                if count > 0:
                    base_user_proba = count / total_user_count
                    max_user_proba = max(base_user_proba, max_user_proba)
                    user_proba = base_user_proba * min(1, (count / N) * math.exp(-0.7 * age_coef))
                    details[word]["user_proba"] = user_proba

                elif count < 0:
                    details[word]["user_proba_dec"] = 1 / 3.16 ** (abs(count) * math.exp(-0.7 * age_coef))

        # -- stock probability --
        score_count = dict()
        for sc in all_scores:
            score_count[sc] = 0
            for word in details:
                if not details[word]: continue
                if curve_scores and curve_scores[word] < max_curve_score - flt_curve_score_threshold:
                    continue  # you should have typed it better :-)
                if details[word]["probas"].get("s1", 0) < max_s1 / flt_word_max_ratio: continue

                score_count[sc] += details[word]["count"].get(sc, 0)

        # find which scores are relevant in the current context
        cluster_score_id = word_score_id = None

        for sc in all_scores:
            count = score_count.get(sc, 0)
            if count > flt_min_count:
                if sc.startswith("c"):
                    if not cluster_score_id: cluster_score_id = sc
                else:
                    if not word_score_id: word_score_id = sc

        sc1, sc2 = word_score_id, None

        if cluster_score_id and word_score_id:
            if int(word_score_id[-1:]) < int(cluster_score_id[-1:]): sc1, sc2 = cluster_score_id, word_score_id

        if not sc1: sc1 = cluster_score_id or "s1"

        # evaluate score
        result = dict()
        max_proba = 0

        sc2index = [ None ] + list(reversed(all_scores))

        # 2 word scores can be compared if they have the same type, otherwise the smaller one is irrelevant
        score_num_type = sc2index.index(sc1) * 10 + sc2index.index(sc2)
        if return_score_num_type is not None: return_score_num_type.append(score_num_type)  # @todo this is probably invalid if we use user scores (?)

        for word in details:
            if not details[word]: continue
            proba = details[word]["probas"].get(sc1, 0)
            details[word]["filter"] = dict(proba = proba, coef = (sc1, sc2), score_num_type = score_num_type)

            # if sc2 and proba:
            #     proba2 = details[word]["probas"].get(sc2, 0)
            # ...
            # (this case will be probably taken care with the "s1" filter above
            #  so wait for real-life occurence of a bad-s2, good-s1 case)

            proba *= details[word].get("user_proba_dec", 1)  # reduce probability if user has refused this word in this context

            details[word]["filter"]["proba_stock"] = proba

            details[word]["filter"]["proba"] = proba
            max_proba = max(max_proba, proba)

        # -- aggregation between user and stock probabilities --
        for word in details:
            final_score = 0
            if not details[word]: continue
            proba = details[word]["filter"]["proba"]
            user_proba = details[word].get("user_proba", None)
            if proba or user_proba:
                xs = math.log10(max_proba / proba) if proba else 1000
                xu = math.log10(max_user_proba / user_proba) if user_proba and user_proba > 0 else 1000

                x = min(xu, xs)
                if xu < xs: details[word]["filter"]["use_user_proba"] = 1

                y = self._filter_func(x)
                final_score = flt_max - flt_coef * y
                details[word]["filter"]["xu"] = xu
                details[word]["filter"]["xs"] = xs
                details[word]["filter"]["y"] = y

            result[word] = (final_score, details[word])

        return result

    def _guess1(self, context, words, correlation_id = -1, verbose = False, quality_index = 0.5, return_score_num_type = None):
        """ internal method : prediction engine entry point (stateless)
            input : dictionary: word => WordScore
            output : dictionary: word => WordScore w/ additional attributes
        """

        if verbose: self.log("guess1: %s %s" % (context, list(words.keys())))

        # these settings are completely empirical (or just random guesses), and they probably depend on end user
        coef_score_upper = self.cf('score_upper', 0.01, float)

        max_star_index = max([x.cls for x in words.values()] + [ -1 ])

        star_bonus = 0
        if max_star_index >= 0 and max_star_index <= self.cf('max_star_index', 8, int):
            star_bonus = self.cf('star_bonus', 0.1, float)  # it's low enough, we don't need this: / (1 + max_star_index)

        no_new_word = (max_star_index > self.cf('max_star_index_new_word', 5, int))

        all_words = sorted(words.keys(),
                           key = lambda x: words[x].score + (star_bonus if words[x].star else 0),
                           reverse = False)

        max_score_pow = self.cf('max_score_pow', 1, int)
        max_score = max([ i.score for w, i in words.items() ])

        max_words = self.cf('predict_max_candidates', 25, int)
        all_words = all_words[-max_words:]

        # requests all scores using bulk requests
        all_predict_scores = self._get_all_predict_scores(all_words, context,
                                                          return_score_num_type = return_score_num_type,
                                                          curve_scores = dict([ (w, s.score) for w, s in words.items() ]))

        new_words = dict()
        for word in all_words:
            score_predict, predict_detail = all_predict_scores.get(word, (0, dict()))
            coef_score_predict = self.coef_score_predict[0] * (1 - quality_index) + self.coef_score_predict[1] * quality_index

            # overall score
            score = words[word].score * (max_score ** max_score_pow) + coef_score_predict * score_predict

            if word[0].isupper(): score -= coef_score_upper


            if words[word].star: score += star_bonus
            if no_new_word and not score_predict: score -= star_bonus

            message = "%s: %.3f = %.3f[%3d%s] + %.3f * %.3f[%s]" % (
                word, score,
                words[word].score, words[word].cls,
                "*" if words[word].star else " ",
                coef_score_predict, score_predict, re.sub(r'(\d\.0*\d\d)\d+', lambda m: m.group(1), str(predict_detail)))

            new_words[word] = WordScore.copy(words[word])
            new_words[word].message = message
            new_words[word].final_score = score
            new_words[word].word = word
            new_words[word].score_predict = score_predict
            new_words[word].coef_score_predict = coef_score_predict
            new_words[word].score_no_predict = score - coef_score_predict * score_predict
            new_words[word].predict_detail = predict_detail
            new_words[word].quality_index = quality_index

        return new_words


    def _quality_index_update(self):
        self._speed_hist.update()
        self._score_hist.update()

    def _quality_index(self, max_score, speed):
        if not max_score: return 0.5, "fallback"
        if not self.hist_file and self.dbfile:
            self.hist_file = os.path.join(os.path.dirname(self.dbfile), "hist.pck")
            self._quality_index_load(self.hist_file)

        i_speed = 1 - self._speed_hist.normalize(speed)
        i_score = self._score_hist.normalize(max_score)
        self._hist_dirty = True

        value = min(1, max(0, 1.6 * (i_speed + i_score) / 2 - .3))
        return value, "sp=%.2f(%d-%d)+sc=%.2f(%.2f-%.2f)->%.2f" % (
            i_speed, self._speed_hist.low, self._speed_hist.high,
            i_score, self._score_hist.low, self._score_hist.high,
            value)

    def _quality_index_load(self, fname):
        try:
            with open(fname, "rb") as f:
                (h1, h2, version) = pickle.load(f)
                if version == HistPercentile.VERSION:
                    (self._speed_hist, self._score_hist) = (h1, h2)
            self._hist_dirty = False
        except: pass  # no problem

    def _quality_index_save(self, fname):
        if self._hist_dirty:
            with open(fname, "wb") as f:
                pickle.dump((self._speed_hist, self._score_hist, HistPercentile.VERSION), f, pickle.HIGHEST_PROTOCOL)
            self._hist_dirty = False

    def guess(self, matches, correlation_id = -1, speed = -1):
        """ main entry point for predictive guess """

        self.log('[*] ' + time.ctime() + " - TS: " + self.mod_ts)
        t0 = time.time()

        self._update_last_words()
        self.last_use = time.time()

        words = dict()
        display_matches = []
        for x in matches:
            for y in x[4].split(','):
                word = x[0] if y == '=' else y
                words[word] = WordScore(x[1], x[2], x[3])  # score, class, star
                display_matches.append((word, words[word]))

        max_score = max( [ words[w].score for w in words ] ) if words else 0

        quality_index, quality_detail = self._quality_index(max_score, speed)

        self.log("ID: %d - Speed: %d - Context: %s - Quality: [%s] - Settings: [cp=%.2f,%.2f]" %
                 (correlation_id, speed,  self.last_words, quality_detail, self.coef_score_predict[0], self.coef_score_predict[1]))
        self.log("Matches: %s" % ','.join("%s[%s]" % x for x in reversed(display_matches)))

        if not words: return

        if not self.db:
            # in case we have no prediction DB (this case is probably useless)
            self.predict_list = sorted(words.keys(), key = lambda x: words[x].score, reverse = True)
            if self.predict_list: return self.predict_list.pop(0)
            return None

        self.db.clear_stats()

        words = self._guess1(self.last_words, words, correlation_id = correlation_id, quality_index = quality_index)

        lst = list(words.keys())
        lst.sort(key = lambda w: words[w].final_score, reverse = True)

        i = 0
        for w in lst:
            i += 1
            if i > 2 and not words[w].score_predict: continue
            self.log("- " + words[w].message)

        self.predict_list = lst[0:self.cf('max_predict', 30, int)]

        if self.predict_list:
            word = self.predict_list.pop(0)
        else:
            word = None

        t = int((time.time() - t0) * 1000)
        self.log("Selected word:", word, "- elapsed:", t, "- db_time:", int(1000 * self.db.timer), "- db_count:", self.db.count)
        self.log()
        self.last_guess = word

        # store result for later user correction
        if word:
            key = ':'.join([ word ] + self.last_words[:2])
            h = dict(ts = t0,
                     words = words,
                     ordered_list = lst,
                     guess = word,
                     context = self.last_words)
            self.guess_history[key] = h
            self.guess_history_last = [ h ] + self.guess_history_last[:10]

        # done :-)
        return word

    def backtrack(self, correlation_id = -1, verbose = False, expected = None, top = 3):
        """ After a word guess, try to correct previous words
            This is intended to be called asynchronously after a simple guess
            (see above)
            This function returns information describing how to patch the text
            and to check if modifications are still valid (in the meantime user
            may have invalidated the new guess) """

        if not self.db: return
        if not self.cf("backtrack_enable", 0, int): return  # broken for now --> disabled by default

        self.db.clear_stats()

        if len(self.guess_history_last) < 2: return

        h0 = self.guess_history_last[0]
        h1 = self.guess_history_last[1]

        # @todo check new sentence ?

        if not h0["context"] or h0["context"][0] == '#START': return  # starting a new sentence

        if os.getenv("BT_DUMMY", None):
            # return dummy values (just for GUI part testing)
            self._learn(False, h0["guess"], h0["context"])
            self._learn(False, h1["guess"], h1["context"])
            return ("foo", "bar", h0["context"][0], h0["guess"], correlation_id, h1["context"] and h1["context"][0] == '#START')

        list_lower = lambda x: [ w.lower() for w in x ]
        if list_lower(h0["context"]) != list_lower([ h1["guess"] ] + h1["context"]): return

        def get_words_and_score(h, max_words = None):
            words = list(h["words"].keys())
            words.sort(key = lambda x: h["words"][x].score, reverse = True)  # sort by curve score only
            score = h["words"][h["guess"]].final_score
            if max_words: words = words[:max_words]
            return words, score

        def add_scores(ws1, ws0):
            if ws1 is None or ws0 is None: return -1.0, -1.0

            predict_score = ws1.score_predict + ws0.score_predict
            score = ws1.score_no_predict + ws0.score_no_predict + ((ws0.coef_score_predict + ws1.coef_score_predict) / 2) * predict_score
            return score, predict_score

        t0 = time.time()

        max_words = self.cf('max_words_backtrack', 15, int)
        backtrack_score_threshold = self.cf('backtrack_score_threshold', 0.05, float)
        backtrack_predict_threshold = self.cf('backtrack_predict_threshold', 0.1, float)

        # unfortunately with new score aggregation function, we need to check all words (result depend on the full set of possible words)
        words0, score0 = get_words_and_score(h0)  # no limitation to 'maw_words', cf. above comment
        words1, score1 = get_words_and_score(h1, max_words)

        dict_word0 = dict([ (w, h0["words"][w]) for w in words0 ])

        found = None
        guess_score, guess_predict_score = add_scores(h1["words"][h1["guess"]], h0["words"][h0["guess"]])
        max_score = guess_score

        def show_score(h, w = None):
            if not h: return "%s(-)" % w
            if not w: w = h["guess"]
            if w in h["words"]:
                sc = h["words"][w]
                return "%s(%.3f+%.3f=%.3f)" % (w, sc.score_no_predict, sc.coef_score_predict * sc.score_predict, sc.final_score)
            else:
                return "%s(-)" % w

        self.log("Backtrack: [%.3f] %s + %s [Guess]" % (guess_score, show_score(h1), show_score(h0)))
        h0_new = dict()
        loops = 0
        messages = dict()

        _d0 = h0["words"][h0["guess"]].predict_detail.get("filter", None)
        max_score_num_type = _d0["score_num_type"] if _d0 else -1

        for w1 in words1:
            if w1 == h1["guess"]:
                h0_new[w1] = dict(h0)
                continue

            tmp = [ ]
            words = self._guess1([ w1 ] + h1["context"], dict_word0, verbose = verbose, return_score_num_type = tmp)
            score_num_type = tmp[0]

            if score_num_type > max_score_num_type:
                # we find a higher score type, drop previous maxima
                max_score_num_type = score_num_type
                max_score = 0
            elif score_num_type < max_score_num_type:
                # we have a lower score type --> just ignore this score
                continue

            h0_new[w1] = dict(words = words)
            for w0 in words:
                loops += 1
                score, predict_score = add_scores(h1["words"][w1], words[w0])
                if verbose: self.log(" [%.2f] %s(%.2f) + %s" % (score, w1, h1["words"][w1].final_score, words[w0].message))

                messages[score] = "[%.3f] %s + %s" % (score, show_score(h1, w1), show_score(h0_new[w1], w0))

                if score > max_score and predict_score > guess_predict_score + backtrack_score_threshold \
                  and h1["words"][w1].score_predict > h1["words"][h1["guess"]].score_predict - backtrack_predict_threshold \
                  and words[w0].score_predict > h0["words"][h0["guess"]].score_predict - backtrack_predict_threshold:
                    max_score, found = score, (w0, w1)

        for k in sorted(messages.keys(), reverse = True)[:top]:
            self.log("Backtrack: %s" % messages[k])

        message = "[%s] %s %s" % (",".join(reversed(h1["context"]))[-30:], show_score(h1), show_score(h0))
        if found: message += " <%.3f> ==> [BT-OK] %s %s <%.3f>" % (guess_score, show_score(h1, found[1]), show_score(h0_new[found[1]], found[0]), max_score)
        else: message += " ==> [no change]"
        if expected:
            message += " - Expected = %s %s <%.3f>" % (show_score(h1, expected[0]),
                                                       show_score(h0_new.get(expected[0]), expected[1]),
                                                       add_scores(h1["words"].get(expected[0], None),
                                                                  h0_new[expected[0]]["words"].get(expected[1], None))[0] if expected[0] in h0_new else -1.0)

            if found: message += " =%s" % ("OK" if found and (found[0] == expected[1] and found[1] == expected[0]) else "FAIL")
        t = int((time.time() - t0) * 1000)
        self.log("Backtrack:", message, "- elapsed:", t, "- db_time:", int(1000 * self.db.timer), "- db_count:", self.db.count, "- loops:", loops)
        self.log()

        if not found: return

        # update learning info (as backtracking is intended to be very rare, just unlearn the two guessed words)
        self._learn(False, h0["guess"], h0["context"])
        self._learn(False, h1["guess"], h1["context"])

        # return value: new word1, new word2, expected word1 (capitalized), expected word2 (capitalized), correlation_id, capitalize 1st word (bool)
        return (found[1], found[0], h0["context"][0], h0["guess"], correlation_id, h1["context"] and h1["context"][0] == '#START')


    def cleanup(self, force_flush = False):
        """ periodic tasks: cache purge, DB flush to disc ... """

        if not self.db: return False

        self._commit_learn(commit_all = force_flush)

        now = int(time.time())
        for key in list(self.guess_history):
            if self.guess_history[key]["ts"] < now - 600: del self.guess_history[key]

        # purge
        last_purge = self.db.get_param("last_purge", 0, int)
        if now > last_purge + self.cf("purge_every", 432000, int):
            self.log("Purge DB ...")

            current_day = int(now / 86400)
            self.db.purge(self.cf("purge_min_count1", 3, float), current_day - self.cf("purge_min_date1", 30, int))
            self.db.purge(self.cf("purge_min_count2", 20, float), current_day - self.cf("purge_min_date2", 180, int))

            self.db.set_param("last_purge", now)

        # save learning info to database
        if self.db and len(self.learn_history) == 0 and self.db.dirty:
            self.log("Flushing DB ...")
            self.db.save()  # commit changes to DB

        # if self.last_use < now - 120:
        #    self.log("Unloading in memory DB ...")
        #    self.db.clear()

        # quality metrics
        self._quality_index_update()

        call_me_back = (len(self.learn_history) > 0)  # or self.db.loaded  # ask to be called again later
        if not call_me_back: self._quality_index_save(self.hist_file)

        return call_me_back

    def close(self):
        """ Close all resources & flush data """
        if self.db:
            self.cleanup(force_flush = True)
            self.db.close()
            self.db = None

    def get_version(self):
        try:
            with open(os.path.join(os.path.dirname(__file__), "engine.version"), "r") as f:
                return f.read().strip()
        except:
            return "unknown"
