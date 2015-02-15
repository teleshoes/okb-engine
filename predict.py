#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Simple prediction algorithm based on N-grams and word clustering """

import os
import time
import re
import math
import cfslm, cdb

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
        cdb.set_string("param-%s" % key, str(value))
        self.params[key] = float(value)
        self.dirty = True

    def get_param(self, key, default_value = None):
        if key in self.params: return self.params[key]
        value = cdb.get_string("param-%s" % key)
        if value is None: value = default_value
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
        if not grams: return
        self.dirty = True

        _start = time.time()

        for ids, values in grams.items():
            (stock_count_not_used, user_count, user_replace, last_time) = values  # obviously stock_count is ignored

            cdb.set_gram(ids, float(user_count), float(user_replace), int(last_time))

        self.timer += time.time() - _start
        self.count += 1


    def add_word(self, word):
        self._load()
        self.dirty = True

        _start = time.time()

        lword = word.lower()
        words = cdb.get_words(lword)
        if not words: words = dict()

        if word not in words:
            wid_str = cdb.get_string("word_id")
            wid = int(wid_str) if wid_str else 1000000

            wid += 1
            cdb.set_string("word_id", str(wid))

            words[word] = (wid, 0)  # new words do not belong a cluster
            cdb.set_words(lword, words)

        self.timer += time.time() - _start
        self.count += 1


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

    def close(self):
        self.clear()

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
        self.recent_contexts = []
        self.learn_history = dict()
        self.first_word = None
        self.mock_time = None
        self.guess_history = dict()
        self.coef_score_predict = None
        self.last_use = 0

        self.mod_ts = time.ctime(os.stat(__file__).st_mtime)

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

    def refresh_db(self):
        # dummy loading to "wake-up" db indexes
        if self.db:
            self.log("DB refresh...")
            now = time.time()
            self.db.refresh()
            self.log("DB refresh done (%2f s)" % (time.time() - now))

    def load_db(self):
        """ load database if needed """
        if not self.db:
            if os.path.isfile(self.dbfile):
                self.db = FslmCdbBackend(self.dbfile)
                self.log("DB open OK:", self.dbfile)
                self.refresh_db()

                self.coef_score_predict = self.cf("predict_coef", 1, float)

            else:
                self.log("DB not found:", self.dbfile)

    def update_preedit(self, preedit):
        """ update own copy of preedit (from maliit) """
        self.preedit = preedit

    def _commit_learn(self, commit_all = False):
        if not self.learn_history: return

        t0 = time.time()
        self.db.clear_stats()

        current_day = int(self._now() / 86400)
        half_life = self.cf('learning_half_life', 180, int)

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
                coef = math.exp(-0.7 * (current_day - last_time) / float(half_life))
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

        wordl = [ word ] + context

        if not silent:
            self.log("Learn:", "add" if add else "remove", wordl, "{replaces %s}" % replaces if replaces else "")

        wordl = wordl[0:3]
        key = '_'.join(wordl)
        if add:
            self.learn_history[key] = (True, wordl, now, pos)  # add occurence
            if replaces and replaces.upper() != word.upper():  # ignore replacement by the same word
                key2 = '_'.join(([ replaces ] + context)[0:3])
                self.learn_history[key2] = (False, [ replaces ] + context, now, pos)  # add replacement occurence (failed prediction)
        else:
            if key in self.learn_history and not self.learn_history[key][0]:
                pass  # don't remove word replacements
            else:
                self.learn_history.pop(key, None)


    def update_surrounding(self, text, pos):
        """ update own copy of surrounding text and cursor position """
        self.surrounding_text = text
        self.cursor_pos = pos

        self.last_use = time.time()

        verbose = self.cf("verbose", False, bool)

        if pos == -1: return  # disable recording if not in a real text box

        def only_current_sentence(text):
            return re.sub(r'[\.\!\?]', ' #START ', text)

        list_before = [ x for x in re.split(r'[^\w\'\-\#]+', only_current_sentence(self.last_surrounding_text)) if x ]
        list_after = [ x for x in re.split(r'[^\w\'\-\#]+', only_current_sentence(self.surrounding_text)) if x ]

        if list_after == list_before: return

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

    def replace_word(self, old, new):
        """ inform prediction engine that a guessed word has been replaced by the user """
        # note: this can not be reliably detected with surrounding text only
        # -> case i've got a one word sentence and replace it --> and can't make the difference with moving to another location in the text

        self._learn(True, new, self.last_words, replaces = old)

        if not self.cf('predict_auto_tune', True, bool): return

        # use replacement information to auto-tune prediction weight
        oldkey = ':'.join([ old ] + self.last_words[:2])
        if oldkey in self.guess_history:
            gh = self.guess_history[oldkey]
            if new in gh["words"]:
                gh["replaced"] = new
                newkey = ':'.join([ new ] + self.last_words[:2])
                self.guess_history[newkey] = gh
                del self.guess_history[oldkey]

            infonew = gh["words"][new]
            infoguess = gh["words"][gh["guess"]]

            # update coefficients
            increase = decrease = False
            if infoguess.coef_score_predict * infoguess.score_predict < infoguess.coef_score_predict * infonew.score_predict:
                increase = True
            if infoguess.score_no_predict < infonew.score_no_predict:
                decrease = True

            if increase != decrease:
                name, sign = ("increase", 1) if increase else ("decrease", -1)
                self.coef_score_predict += sign * self.cf("score_predict_tune_increment", 0.005, float)
                self.coef_score_predict = max(0.2, min(5, self.coef_score_predict))
                self.log("Replacement [%s -> %s] - Predict coef %s, new value: %.3f" %
                         (old, new, name, self.coef_score_predict))

                self.db.set_param("predict_coef", self.coef_score_predict)


    def get_predict_words(self):
        """ return prediction list, in a format suitable for QML ListModel """
        return [ { "text": word } for word in self.predict_list ]

    def ping(self):
        """ hello """
        self.log("ping\n")
        return 1

    def _update_last_words(self):
        self.last_words, self.sentence_pos = self._get_last_words(2)

    def _get_last_words(self, count, include_preedit = True):
        """ internal method : compute list of last words for n-gram scoring """

        # if no context available, handle this as isolated words instead of the beginning of a new sentence (so no #START)
        if self.cursor_pos == -1: return [], -1

        text_before = self.surrounding_text[0:self.cursor_pos] + (" " + self.preedit) if include_preedit else ""
        pos = text_before.find('.')
        if pos > -1: text_before = text_before[pos + 1:]

        words = reversed(re.split(r'[^\w\'\-]+', text_before))
        words = [ x for x in words if len(x) > 0 ]

        pos = len(words)
        words = (words + [ '#START' ] * count)[0:count]
        return words, pos


    def _get_all_predict_scores(self, words):
        """ get n-gram DB information (use 2 SQL batch requests instead of a ton of smaller ones, for huge performance boost) """
        # these are called "scores", but they try to be probabilities in the range [0, 1]
        result = dict()
        if not words: return result

        # get previous words (surrounding text) for lookup
        words_set = set(words)
        for word in self.last_words: words_set.add(word)
        words_set.add('#START')

        # idenfity words that would need to be looked-up lowercase first (first words in sentences)
        try_lower_first = None
        if self.sentence_pos == 0:
            try_lower_first = set(words)  # first word -> all candidates
        elif self.sentence_pos == 1 and len(self.last_words) >= 1:
            try_lower_first = set([self.last_words[0]])
        elif self.sentence_pos == 2 and len(self.last_words) >= 2:
            try_lower_first = set([self.last_words[1]])

        # lookup all words from DB or cache (surrounding & candidates)
        word2id = self.db.get_words(words_set, lower_first_words = try_lower_first)

        # get id for previous words
        last_wids = []
        last_cids = []
        for word in self.last_words:
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
            todo[word][score_name] = (num, den, cluster)

        todo = dict()
        ids_list = set(["-2:-1:-1"])
        word2cid = dict()
        for word in words:
            result[word] = (0, dict())
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

        # initialize parameters
        score_parts = dict(s3 = 0.8, c3 = 0.4, s2 = 0.3, c2 = 0.03, s1 = 0.2)  # default values, per language override can be set in DB parameters
        score_coef = dict()
        for part, default_coef in score_parts.items():
            score_coef[part] = self.db.get_param("coef_predict_%s" % part, default_coef)

        # process request result & compute scores
        current_day = int(self._now() / 86400)

        (global_stock_count, global_user_count) = sqlresult["-2:-1:-1"][0:2]  # grand total #NA:#NA:#TOTAL

        # evaluate score components
        for word in todo:
            score = dict()
            detail = dict(scores = dict())
            final_score = 0

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

            for score_name in todo[word]:
                if score_name == "c1": continue  # useless

                num_id, den_id, cluster = todo[word][score_name]
                num = sqlresult.get(num_id, None)
                if not num: continue
                den = sqlresult.get(den_id, None)
                if not den: continue
                detail_append = ""

                # add P(Wi|Ci) term for cluster n-grams
                coef = 1.0
                if cluster:
                    # this is a "c*" score (cluster)
                    if not coef_wc: continue
                    coef = coef_wc
                    detail_append += " coef=%.5f" % coef

                # stock stats
                (stock_count, user_count, user_replace, last_time) = num
                (total_stock_count, total_user_count, dummy1, dummy2) = den

                stock_proba = proba = 1.0 * coef * stock_count / total_stock_count if total_stock_count > 0 else 0

                # @todo learning stuff here :-)

                score1 = score_coef[score_name] * (math.log10(proba) + 8 if proba > 1E-8 else 0) / 8  # [0, 1]
                final_score = max(final_score, score1)

                # only for logging
                detail[score_name] = "%.2e ~ stock=%.2e[%d]%s" % (score1, proba, stock_count, detail_append)
                detail["scores"][score_name] = score1

            if final_score:
                result[word] = (final_score, detail)

        self.recent_contexts = self.recent_contexts[1:10] + [ (word, list(self.last_words)) ]

        return result

    def guess(self, matches, correlation_id = -1, speed = -1):
        """ main entry point for predictive guess """

        self.last_use = time.time()

        class word_score:
            def __init__(self, score, cls, star):
                (self.score, self.cls, self.star) = (score, cls, star)

            def __str__(self):
                return (("%.2f" % self.score)
                        + ((":%d" % self.cls) if self.cls else "")
                        + ("*" if self.star else ""))

        words = dict()
        display_matches = []
        for x in matches:
            for y in x[4].split(','):
                word = x[0] if y == '=' else y
                words[word] = word_score(x[1], x[2], x[3])  # score, class, star
            display_matches.append((x[0], word_score(x[1], x[2], x[3])))

        if not self.db:
            # in case we have no prediction DB (this case is probably useless)
            self.predict_list = sorted(words.keys(), key = lambda x: words[x].score, reverse = True)
            if self.predict_list: return self.predict_list.pop(0)
            return None

        self.db.clear_stats()

        self.log('[*] ' + time.ctime() + " - Version: " + self.mod_ts)
        t0 = time.time()
        self._update_last_words()
        if not len(matches): return

        self.log("ID: %d - Speed: %d - Context: %s - Matches: %s" %
                 (correlation_id, speed,  self.last_words,
                  ','.join("%s[%s]" % x for x in reversed(display_matches))))

        # these settings are completely empirical (or just random guesses), and they probably depend on end user
        coef_score_upper = self.cf('score_upper', 0.01, float)
        coef_score_sign = self.cf('score_sign', 0.01, float)

        max_star_index = max([x.cls for x in words.values()] + [ -1 ])

        star_bonus = 0
        if max_star_index >= 0 and max_star_index <= self.cf('max_star_index', 8, int):
            star_bonus = self.cf('star_bonus', 1.0, float) / (1 + max_star_index)

        no_new_word = (max_star_index > self.cf('max_star_index_new_word', 5, int))

        all_words = sorted(words.keys(),
                           key = lambda x: words[x].score + (star_bonus if words[x].star else 0),
                           reverse = False)

        max_score_pow = self.cf('max_score_pow', 1, int)
        max_score = max([ i.score for w, i in words.items() ])

        max_words = self.cf('predict_max_candidates', 25, int)
        all_words = all_words[-max_words:]

        all_predict_scores = self._get_all_predict_scores(all_words)  # requests all scores using bulk requests

        lst = []
        for word in all_words:
            score_predict, predict_detail = all_predict_scores.get(word, (0, dict()))
            coef_score_predict = self.coef_score_predict

            # overall score
            score = words[word].score * (max_score ** max_score_pow) + coef_score_predict * score_predict

            if word[0].isupper(): score -= coef_score_upper
            if word.find("'") > -1 or word.find("-") >  1: score -= coef_score_sign


            if words[word].star: score += star_bonus
            if no_new_word and not score_predict: score -= star_bonus

            message = "%15s : %.3f = %.3f[%3d%s] + %.3f * %.3f[%s]" % (
                word, score,
                words[word].score, words[word].cls,
                "*" if words[word].star else " ",
                coef_score_predict, score_predict, predict_detail)

            words[word].message = message
            words[word].final_score = score
            words[word].word = word
            words[word].score_predict = score_predict
            words[word].coef_score_predict = coef_score_predict
            words[word].score_no_predict = score - coef_score_predict * score_predict
            lst.append(word)

        lst.sort(key = lambda w: words[w].final_score, reverse = True)

        for w in lst: self.log(words[w].message)

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
            self.guess_history[key] = dict(ts = t0,
                                           words = words,
                                           guess = word)

        # done :-)
        return word

    def cleanup(self, force_flush = False):
        """ periodic tasks: cache purge, DB flush to disc ... """

        self._commit_learn(commit_all = force_flush)

        now = time.time()
        for key in list(self.guess_history):
            if self.guess_history[key]["ts"] < now - 600: del self.guess_history[key]

        if self.db and len(self.learn_history) == 0 and self.db.dirty:
            self.log("Flushing DB ...")
            self.db.save()  # commit changes to DB

        #if self.last_use < now - 120:
        #    self.log("Unloading in memory DB ...")
        #    self.db.clear()

        return len(self.learn_history) > 0  # or self.db.loaded  # ask to be called again later

    def close(self):
        """ Close all resources & flush data """
        self.cleanup(force_flush = True)
        if self.db:
            self.db.close()
            self.db = None

    def get_version(self):
        try:
            with open(os.path.join(os.path.dirname(__file__), "engine.version"), "r") as f:
                return f.read().strip()
        except:
            return "unknown"
