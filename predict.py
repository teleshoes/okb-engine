#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Simple prediction algorithm based on N-grams """

import sqlite3
import os
import time
import re
import math

class Wordinfo:
    def __init__(self, id, cluster_id):
        self.id = id
        self.cluster_id = cluster_id

class SqliteBackend:
    """ this class implement read/write access to prediction database
        as no python/dbm is working on Sailfish (or i missed it), sqlite is used """

    def __init__(self, dbfile):
        self.conn = sqlite3.connect(dbfile)
        self.clear_stats()
        self.cache = dict()

        self.cache['#TOTAL'] = Wordinfo(-2, -2)
        self.cache['#NA'] = Wordinfo(-1, -1)
        self.cache['#START'] = Wordinfo(-3, -3)

        # fetch parameters
        self.params = dict()
        sql = 'SELECT key, value FROM params'
        curs = self.conn.cursor()
        curs.execute(sql)
        for (key, value) in curs.fetchall():
            self.params[key] = float(value)

    def refresh(self):
        # dummy request to load some blocks from filesystem and reduce
        # latency later when user is waiting for an answer
        self.get_words(['#TOTAL', '#NA', '#START'], use_cache = False)
        self.get_grams(["-2:-1:-1", "-2:-3:-1" ], use_cache = False)

    def set_param(self, key, value):
        if key in self.params:
            sql = 'UPDATE params SET value = ? WHERE key = ?'
        else:
            sql = 'INSERT INTO params (value, key) VALUES (?, ?)'
        curs = self.conn.cursor()
        curs.execute(sql, (value, key))
        self.conn.commit()

        self.params[key] = float(value)

    def clear_stats(self):
        self.timer =  self.count = 0

    def get_grams(self, ids_list, use_cache = True):
        self.count += 1
        _start = time.time()

        result = dict()

        sql = 'SELECT id1, id2, id3, stock_count, user_count, user_replace, last_time FROM grams WHERE '  # @todo add columns for automatic learning
        params = []
        first = True
        for ids in ids_list:
            if ids in self.cache and use_cache:
                if self.cache[ids]: result[ids] = self.cache[ids]  # negative caching
            else:
                if not first: sql += ' OR '
                first = False
                sql += "(id1 = ? AND id2 = ? AND id3 = ?)"
                paramlist = ids.split(':')
                if len(paramlist) != 3: raise Exception("Bad parameter list: %s" % ids)
                params.extend(paramlist)

        if params:
            curs = self.conn.cursor()
            curs.execute(sql, tuple(params))

            for row in curs.fetchall():
                (id1, id2, id3, stock_count, user_count, user_replace, last_time) = row
                ids = ':'.join([str(id1), str(id2), str(id3)])
                result[ids] = self.cache[ids] = (stock_count, user_count, user_replace, last_time)

            for ids in [ i for i in ids_list if i not in result ]: self.cache[ids] = None  # negative caching

        self.timer += time.time() - _start
        return result

    def set_grams(self, grams):
        if not grams: return

        _start = time.time()
        curs = self.conn.cursor()
        for ids, values in grams.items():
            (stock_count, user_count, user_replace, last_time) = values  # obviously stock_count is ignored

            # we can issue a request by line, because performance is not critical (asynchronous write to DB)
            # (we can use conn.executemany() if needed)
            self.count += 1
            if ids not in self.cache:
                raise Exception("DB inconsistency?")
            elif self.cache[ids] is not None:
                # update line
                sql = 'UPDATE grams SET user_count = ?, user_replace = ?, last_time = ? WHERE id1 = ? AND id2 = ? AND id3 = ?'
                stock_count = self.cache[ids][0]
            else:
                # create new line
                sql = 'INSERT INTO grams (stock_count, user_count, user_replace, last_time, id1, id2, id3) VALUES (0, ?, ?, ?, ?, ?, ?)'
                stock_count = 0

            self.cache[ids] = tuple([ stock_count, user_count, user_replace, last_time ])

            params = [ user_count, user_replace, last_time ] + ids.split(':')
            curs.execute(sql, tuple(params))

        self.conn.commit()
        self.timer += time.time() - _start


    def get_words(self, words, try_lower_case = True, use_cache = True):
        self.count += 1
        _start = time.time()

        result = dict()

        sql = 'SELECT word, id, cluster_id FROM words WHERE '
        params = []
        first = True
        for word in words:
            if word in self.cache and use_cache:
                if self.cache[word]: result[word] = self.cache[word]  # negative caching use None value and must not be returned
            else:
                if not first: sql += ' OR '
                first = False
                sql += "(word = ?)"
                params.append(word)

        if params:
            curs = self.conn.cursor()
            curs.execute(sql, tuple(params))

            for row in curs.fetchall():
                result[row[0]] = Wordinfo(row[1], row[2])

        self.timer += time.time() - _start

        if try_lower_case:
            not_found_try_lower = [ w for w in words if w not in result and w != w.lower() ]

            if not_found_try_lower:
                # (ugly recursive trick)
                result_lower = self.get_words([ w.lower() for w in not_found_try_lower ], try_lower_case = False)
                for word in not_found_try_lower:
                    lword = word.lower()
                    if lword in result_lower:
                        result[word] = result_lower[lword]

        for word, info in result.items():
            self.cache[word] = info

        for word in [ w for w in words if w not in result ]: self.cache[word] = None  # negative caching

        return result

    def close(self):
        self.conn.close()

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
        self.last_guess = None
        self.recent_contexts = []
        self.learn_history = dict()
        self.first_word = None
        self.mock_time = None
        self.guess_history = dict()
        self.coef_score_predict = dict()

        self.mod_ts = time.ctime(os.stat(__file__).st_mtime)

    def _mock_time(self, time):
        self.mock_time = time

    def _now(self):
        if self.mock_time: return self.mock_time
        return int(time.time())

    def cf(self, key, default_value, cast = None):
        """ "mockable" configuration """
        if self.tools:
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
            self.db.refresh()

    def load_db(self):
        """ load database if needed """
        if not self.db:
            if os.path.isfile(self.dbfile):
                self.db = SqliteBackend(self.dbfile)
                self.log("DB open OK:", self.dbfile)
                self.refresh_db()

                for n in [1, 2, 3]:
                    if "predict_coef_%d" % n in self.db.params:
                        self.coef_score_predict[n] = self.db.params["predict_coef_%d" % n]
                    else:
                        self.coef_score_predict[n] = self.cf('default_score_predict_%d' % n, 0.4 - 0.125 * (3 - n), float)

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
        for key in learn:
            item = self.learn_history[key]
            for word in item[1]:
                word_set.add(word)
        word2id = dict([ (w, i.id) for (w,i) in self.db.get_words(words_set).items() ])

        # list all n-grams lines to update
        na_id = word2id['#NA']
        total_id = word2id['#TOTAL']
        todo = dict()
        for key in set(learn):
            # action == True -> new occurrence / action == False -> error corrected
            (action, words, ts) = self.learn_history[key]
            while words:
                wids = [ word2id.get(x, na_id) for x in words ]

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
                 "db_time:", int(1000 * self.db.timer), "db_count:", self.db.count, "cache:", len(self.db.cache))

    def _learn(self, add, word, context, replaces = None, silent = False):
        if not self.db: return

        now = int(time.time())

        try:
            pos = context.index('#START')
            context = context[0:pos + 1]
        except:
            pass  # #START not found, this is not an error

        wordl = [ word ] + context

        if not silent:
            self.log("Learn:", "add" if add else "remove", wordl, "{replaces %s}" % replaces if replaces else "")

        wordl = wordl[0:3]
        key = '_'.join(wordl)
        if add:
            self.learn_history[key] = (True, wordl, now)  # add occurence
            if replaces and replaces.upper() != word.upper():  # ignore replacement by the same word
                key2 = '_'.join(([ replaces ] + context)[0:3])
                self.learn_history[key2] = (False, [ replaces ] + context, now)  # add replacement occurence (failed prediction)
        else:
            if key in self.learn_history and not self.learn_history[key][0]:
                pass  # don't remove word replacements
            else:
                self.learn_history.pop(key, None)


    def update_surrounding(self, text, pos):
        """ update own copy of surrounding text and cursor position """
        self.surrounding_text = text
        self.cursor_pos = pos

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

            if increase != decrease and infonew.gram:
                name, sign = ("increase", 1) if increase else ("decrease", -1)
                gram = infonew.gram

                self.coef_score_predict[gram] += sign * self.cf("score_predict_tune_increment", 0.002, float)
                self.coef_score_predict[gram] = max(0.001, min(1, self.coef_score_predict[gram]))
                self.log("Replacement [%s -> %s] - Predict coef[%d] %s, new value: %.3f" %
                         (old, new, gram, name, self.coef_score_predict[gram]))

                self.db.set_param("predict_coef_%d" % gram, self.coef_score_predict[gram])


    def get_predict_words(self):
        """ return prediction list, in a format suitable for QML ListModel """
        return [ { "text": word } for word in self.predict_list ]

    def ping(self):
        """ hello """
        self.log("ping\n")
        return 1

    def _update_last_words(self):
        self.last_words = self._get_last_words(2)

    def _get_last_words(self, count, include_preedit = True):
        """ internal method : compute list of last words for n-gram scoring """

        # if no context available, handle this as isolated words instead of the beginning of a new sentence (so no #START)
        if self.cursor_pos == -1: return []

        text_before = self.surrounding_text[0:self.cursor_pos] + (" " + self.preedit) if include_preedit else ""
        pos = text_before.find('.')
        if pos > -1: text_before = text_before[pos + 1:]

        words = reversed(re.split(r'[^\w\'\-]+', text_before))
        words = [ x for x in words if len(x) > 0 ]

        words = (words + [ '#START' ] * count)[0:count]
        return words


    def _get_all_predict_scores(self, words):
        """ get n-gram DB information (use 2 SQL batch requests instead of a ton of smaller ones, for huge performance boost) """
        # these are called "scores", but they try to be probabilities in the range [0, 1]
        result = dict()
        if not words: return result

        # get previous words (surrounding text) for lookup
        words_set = set(words)
        for word in self.last_words: words_set.add(word)
        words_set.add('#START')

        # lookup all words from DB or cache (surrounding & candidates)
        word2id = self.db.get_words(words_set)

        # get id for previous words
        last_wids = []
        last_cids = []
        for word in self.last_words:
            if word not in word2id: break  # unknown word -> only use smaller n-grams
            last_wids.append(word2id[word].id)
            last_cids.append(word2id[word].cluster_id)

        # prepare scores request
        def add(word, todo, ids_list, score_id, lst):
            lst = [ str(x) for x in lst ]
            while len(lst) < 3: lst.append('-1')  # N/A
            num = ':'.join(lst)
            den = ':'.join([ '-2' ] + lst[1:])
            ids_list.add(num)
            ids_list.add(den)
            if word not in todo: todo[word] = dict()
            todo[word][score_id] = (num, den)

        todo = dict()
        ids_list = set(["-2:-1:-1"])
        for word in words:
            result[word] = (0, dict())
            if word not in word2id:  # unknown word -> dummy scoring
                continue
            wid, cid = word2id[word].id, word2id[word].cluster_id

            add(word, todo, ids_list, "s1", [wid])

            if len(last_wids) >= 1:
                add(word, todo, ids_list, "s2", [wid, last_wids[0]])
                if cid and last_cids[0] != 0:
                    add(word, todo, ids_list, "c2", [cid, last_cids[0]])

            if len(last_wids) >= 2:
                add(word, todo, ids_list, "s3", [wid] + last_wids[0:2])
                if cid and last_cids[0] != 0 and last_cids[1] != 0:
                    add(word, todo, ids_list, "c3", [cid] + last_cids[0:2])

        # request
        sqlresult = self.db.get_grams(ids_list)

        # initialize parameters
        lambda1 = self.db.params.get('lambda1', self.cf('default_lambda1', 0.05, float))
        lambda2 = self.db.params.get('lambda2', self.cf('default_lambda2', 0.5, float))
        lambda3 = 1 - lambda1 - lambda2

        # process request result & compute scores
        current_day = int(self._now() / 86400)

        (global_stock_count, global_user_count) = sqlresult["-2:-1:-1"][0:2]  # grand total #NA:#NA:#TOTAL
        learning_count = self.cf('learning_count', 5000, int)
        learning_count_min = self.cf('learning_count_min', 10, int)
        learning_master_switch = self.cf('learning_enable', True, bool)

        for word in todo:
            score = dict()
            detail = dict()
            for score_id in todo[word]:
                num_id, den_id = todo[word][score_id]
                num = sqlresult.get(num_id, None)
                if not num: continue
                den = sqlresult.get(den_id, None)
                if not den: continue
                detail_append = ""

                # stock stats
                (stock_count, user_count, user_replace, last_time) = num
                (total_stock_count, total_user_count, dummy1, dummy2) = den
                stock_proba = 1.0 * stock_count / total_stock_count if total_stock_count > 0 else 0

                # final score for current N value
                if (user_count or user_replace) and total_user_count \
                   and global_user_count and learning_master_switch:
                    # use information learned from user

                    # usage of this context compared to the whole corpus
                    # for 1-grams it's always 1, <= 1 otherwise
                    context_usage = (total_user_count / global_user_count)
                    if global_stock_count and total_stock_count:
                        context_usage = math.sqrt(context_usage * (total_stock_count / global_stock_count))

                    # user text input is way smaller than stock corpus (predict.db) and
                    coef_user = 1 - math.exp(-0.7 * total_user_count / max(learning_count_min, learning_count * context_usage))

                    c = max(1, 1.0 * total_stock_count / total_user_count) * coef_user
                    if not total_stock_count: c = 1.0

                    proba = ((stock_count + c * user_count * user_count / (user_count + user_replace))
                             / (total_stock_count + c * total_user_count + learning_count))
                    # ^^^ learning count in denominator is for smoothing in case of word not in stock

                    detail_append = (" user=[%d-%d/%d] age=%d [learn: ctx=%.2e coef=%.2e proba=%.2e->%.2e]" %
                                     (user_count, user_replace, total_user_count, current_day - last_time, context_usage, coef_user, stock_proba, proba))

                else:
                    # only use stock information
                    proba = stock_proba

                score[score_id] = max(0, proba)  # make sure user_replace does not lead to negative result

                # only for logging
                detail[score_id] = "stock=%.2e[%d/%d]%s" % (stock_proba, stock_count, total_stock_count, detail_append)

            # use linear interpolation between probabilities for each N value (cf. Jurasky & Martin §4.6: interpolation)
            final_score, n = None, 0
            if "s3" in score:
                final_score = lambda1 * score.get("s1", 0) + lambda2 * score.get("s2", 0) + lambda3 * score["s3"]
                n = 3
            elif "s2" in score:  # context size is 2 (e.g. first word in a sentence)
                final_score = (lambda1 * score.get("s1", 0) + lambda2 * score["s2"]) / (lambda1 + lambda2)
                n = 2
            elif "s1" in score:  # context size in 1 (e.g. first word after an unknown word)
                final_score = score["s1"]
                n = 1

            if final_score:
                if n: detail["gram"] = n
                result[word] = (final_score, detail)  # we need smoothing

        self.recent_contexts = self.recent_contexts[1:10] + [ (word, list(self.last_words)) ]

        return result

    def guess(self, matches, correlation_id = -1, speed = -1):
        """ main entry point for predictive guess """

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

        max_words = self.cf('predict_max_candidates', 25, int)
        all_words = all_words[-max_words:]

        all_predict_proba = self._get_all_predict_scores(all_words)  # requests all scores using bulk requests

        lst = []
        for word in all_words:
            predict_proba, detail_predict = all_predict_proba.get(word, (0, dict()))

            score_predict = (math.log10(predict_proba) + 8 if predict_proba > 1E-8 else 0) / 8  # [0, 1]

            # overall score
            gram = detail_predict.get("gram", None)
            score = words[word].score
            coef_score_predict = self.coef_score_predict.get(gram, 0)
            score += coef_score_predict * score_predict
            if word[0].isupper(): score -= coef_score_upper
            if word.find("'") > -1 or word.find("-") >  1: score -= coef_score_sign


            if words[word].star: score += star_bonus
            if no_new_word and not score_predict: score -= star_bonus

            message = "score[%s]: %.3f[%d%s] + %.3f * %.3f[%s] --> %.3f" % (
                word,
                words[word].score, words[word].cls,
                "*" if words[word].star else "",
                coef_score_predict, score_predict, detail_predict, score)

            words[word].message = message
            words[word].final_score = score
            words[word].predict_proba = predict_proba
            words[word].word = word
            words[word].score_predict = score_predict
            words[word].coef_score_predict = coef_score_predict
            words[word].score_no_predict = score - coef_score_predict * score_predict
            words[word].gram = gram
            lst.append(word)

        lst.sort(key = lambda w: words[w].final_score, reverse = True)

        for w in lst: self.log(words[w].message)

        self.predict_list = lst[0:self.cf('max_predict', 30, int)]

        if self.predict_list:
            word = self.predict_list.pop(0)
        else:
            word = None

        t = int((time.time() - t0) * 1000)
        self.log("Selected word:", word, "- elapsed:", t, "- db_time:", int(1000 * self.db.timer), "- db_count:", self.db.count, "- cache:", len(self.db.cache))
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

        if self.db and not self.learn_history and len(self.db.cache) > self.cf("max_cache", 10000, int):
            # emergency cache flush. @todo replace it with a proper LRU implementation
            self.log("DB cache flush")
            self.db.cache = dict()
            self.refresh_db()

        now = time.time()
        for key in list(self.guess_history):
            if self.guess_history[key]["ts"] < now - 600: del self.guess_history[key]

        return len(self.learn_history) > 0  # ask to be called again later

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
