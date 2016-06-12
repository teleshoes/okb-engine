#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Language model for prediction engine """

import time
import re
import unicodedata
import math

def word2letters(word):
    letters = ''.join(c for c in unicodedata.normalize('NFD', word.lower()) if unicodedata.category(c) != 'Mn')
    letters = re.sub(r'[^a-z]', '', letters.lower())
    letters = re.sub(r'(.)\1+', lambda m: m.group(1), letters)
    return letters

def smallify_number(number):  # display number as 4 characters (for debug output)
    number = int(number)
    if number <= 9999: return "%4d" % int(number)
    if number <= 999999: return "%3dk" % int(number / 1000)
    return "%3dm" % int(number / 1000000)

def smallify_number_3(number):  # display number as 3 characters (for debug output)
    number = int(number)
    if number < 1000: return "%3d" % int(number)
    if number < 100000: return "%2dk" % int(number / 1000)
    if number < 1000000: return ".%1dM" % int(number / 100000)
    return "%2dM" % int(number / 1000000)

def split_old_candidate_list(candidates):
    """ utility function, split candidate list in old format (JSON output)
        (one item per curve match) to new format (one item per distinct
        candidates, as a dict(candidate -> score) """
    out = dict()

    for c in candidates:
        name = c["name"]
        for w in c["words"].split(","):
            word = name if w == "=" else w
            out[word] = c["score"]

    return out


class WordInfo:
    def __init__(self, word, id = -1, cluster_id = -1, displayed_word = None, ts = 0, bad_caps = False):
        self.word = word  # actual word in the database
        self.id = id
        self.cluster_id = cluster_id
        self.displayed_word = displayed_word  # shown word (e.g. with capitalized)
        self.ts = ts
        self.count = dict()
        self.bad_caps = bad_caps

    def __str__(self):
        return "[WordInfo: %s (%d:%d) %s %s %s]" % (self.word, self.id, self.cluster_id,
                                                    self.displayed_word or "-", self.count or "", "[BC]" if self.bad_caps else "")

class Count:
    def __init__(self, *params):
        (self.count, self.user_count, self.total_count,
         self.total_user_count, self.user_replace, self.last_time) = params

    def __str__(self):
        return "[Count: count=%d total=%d user=%d user_total=%d user_replace=%d]" % \
            (self.count, self.total_count, self.user_count, self.total_user_count, self.user_replace)


class LanguageModel:
    def __init__(self, db, tools = None, cfvalues = dict(), debug = False, dummy = True):
        self._db = db
        self._tools = tools
        self._cfvalues = cfvalues
        self._mock_time = None
        self._cache = dict()
        self._learn_history = dict()
        self._last_purge = self._db.get_param("last_purge", 0, int)

        self._half_life = self.cf('learning_half_life', 20, int)
        self._history = []
        self._debug = debug

        if dummy: self._dummy_request()

    def mock_time(self, time):
        self._mock_time = time

    def _now(self):
        return self._mock_time or int(time.time())

    def cf(self, key, default_value = None, cast = None):
        """ "mockable" configuration """
        if self._db and self._db.get_param(key):
            ret = self._db.get_param(key)  # per language DB parameter values
        elif self._tools:
            ret = self._tools.cf(key, default_value, cast)
        elif self._cfvalues:
            ret = self._cfvalues.get(key, default_value)
        else:
            ret = default_value

        if cast: ret = cast(ret)
        return ret

    def log(self, *args, **kwargs):
        """ log with default simple implementation """
        if self._tools:
            self._tools.log(*args, **kwargs)
        else:
            print(' '.join(map(str, args)))

    def debug(self, *args, **kwargs):
        if self._debug:
            self.log('DEBUG:', *args, **kwargs)

    def _dummy_request(self):
        # run somme dummy request to avoid huge processing time for first request
        # (load code & warm-up cache)
        words = [ self._db.get_word_by_id(id) for id in range(11, 20) ]
        words = [ w for w in words if w ]
        context = [ words.pop(0), words.pop(1) ]
        words = dict([ (w, 0.8) for w in words ])
        self.guess(context, words)

    def _commit_learn(self, commit_all = False):
        if not self._learn_history: return

        current_day = int(self._now() / 86400)

        if commit_all:
            learn = self._learn_history.keys()
        else:
            now = int(time.time())
            delay = self.cf('commit_delay', 20, int)
            learn = [ key for key, item in self._learn_history.items() if item[2] < now - delay ]

        if not learn: return

        for key in list(learn):
            (action, wordl, ts, pos) = self._learn_history[key]

            context = list(reversed(wordl))
            wi_context = list()
            if context:
                for w in context[-3:]:
                    self._add_wi_context(w, wi_context, create = (w == context[-1]))

            # @todo handle this case: added word is split in multiple WordInfo items (e.g. compound word)

            for i in range(1, 4):
                rev_wi_context = list(reversed(wi_context))[0:i]
                ids = [ c.id for c in rev_wi_context ]
                ids.extend( [ -1 ] * (3 - len(ids)) )  # pad key to 3 elements

                id_num = ids
                id_den = [ -2 ] + ids[1:]  # #TOTAL tag
                for ids in [ id_num, id_den ]:
                    if ids == id_den and not action: break  # replacing word does not increase denominator counts
                    (stock_count, user_count, user_replace, last_time) = self._db.get_gram(ids) or (0, 0, 0, 0)

                    if not last_time:
                        user_count = user_replace = last_time = 0

                    elif current_day > last_time:
                        # data ageing
                        coef = math.exp(-0.7 * (current_day - last_time) / float(self._half_life))
                        user_count *= coef
                        user_replace *= coef

                    last_time = current_day

                    # action
                    if action: user_count += 1
                    else: user_replace += 1

                    self._db.set_gram(ids, (stock_count, user_count, user_replace, last_time))

                # update cache
                key = ':'.join([ x.word for x in wi_context[-i:] ])
                if key in self._cache:
                    type = "s"  # not learing cluster n-grams for now (for type in [ "c", "s" ]:)
                    score_id = type + str(i)
                    if score_id in self._cache[key]:
                        count_cached = self._cache[key][score_id]
                        count_cached.user_count += 1
                        if action: count_cached.total_user_count += 1
                        # and do not bother with age (cache will be eventually flushed)

            self._learn_history.pop(key, None)

        self.log("Commit: Lines updated:", len(learn))


    def learn(self, add, word, context, replaces = None, silent = False):
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
            self._learn_history[key] = (True, wordl, now, pos)  # add occurence
            if replaces and replaces != word:  # ignore replacement by the same word
                key2 = '_'.join(([ replaces ] + context)[0:3])
                self._learn_history[key2] = (False, [ replaces ] + context, now, pos)  # add replacement occurence (failed prediction)
        else:
            if key in self._learn_history and not self._learn_history[key][0]:
                pass  # don't remove word replacements
            else:
                self._learn_history.pop(key, None)


    def _word2wi(self, word, first_word = False, create = False, uncapitalized = False, parent = True):
        """ convert a word to a WordInfo instances list
            there may be multiple instances, e.g. because
            of word splitting to handle compound words """

        # handle full compound words
        if word.find("'") > -1 or word.find("-") > -1:
            l1 = l2 = None

            # prefixes
            mo = re.match(r'^([^\-\']+[\-\'])(.+)$', word)
            if mo:
                l1 = self._word2wi(mo.group(1), first_word)  # prefix
                if l1: l2 = self._word2wi(mo.group(2), False, create = create, uncapitalized = uncapitalized, parent = False)
                if l1 and l2: return l1 + l2
            # suffixes
            mo = re.match(r'^(.+)([\-\'][^\-\']+)$', word)
            if mo:
                l2 = self._word2wi(mo.group(2), False)  # suffix
                if l2: l1 = self._word2wi(mo.group(1), first_word, create = create, uncapitalized = uncapitalized, parent = False)
                if l1 and l2: return l1 + l2

        ids = self._db.get_words(word.lower())

        # @todo half-typed compound words (this version does not work)
        # if not ids and word[-1] in ["'", "-"] and parent:
        #    if len(word) <= 1: return None
        #    l = self._word2wi(word[:-1], first_word, create = create, uncapitalized = uncapitalized, parent = False)
        #    if l: l[-1].displayed_word = word
        #    return l

        # handle full words
        if not ids:
            if not create: return None
            if re.search(r'\d', word) or not re.match(r'^\w+$', word):
                self.log("Bad word not learned: %s" % word)
                return None

            # @todo handle compound words here

            # learn a new word
            lword = word
            if first_word and word[0].isupper() and word[1:].islower(): lword = word.lower()  # uncapitalize first word
            self.log("Learning new word: %s" % lword)
            id = self._db.add_word(lword)
            return [ WordInfo(lword, id = id, cluster_id = None, displayed_word = word, ts = self._now()) ]

        bad_caps = False
        if uncapitalized:
            # we are processing dictionary words coming from curve matching engine
            # and not surrounding text from text field that may be capitalized (cf. "elif" cases below)
            if word not in ids: return None
            out = word

        elif first_word and word[0].isupper() and (word[1:].islower() or len(word) == 1) and word.lower() in ids:
            # find uncapitalized word (first word only)
            out = word.lower()
        elif word in ids:  # find exact word
            out = word

            # First word should be capitalized, so we will prefer a properly capitalized candidate
            bad_caps = word[0].islower() and first_word

        # in remaining cases, words matched have inconsistent capitalization (so this is just a best effort mode)
        elif word.lower() in ids:
            out = word.lower()
            bad_caps = True
        else:
            out = list(ids.keys())[0]  # random choice
            bad_caps = True

        return [ WordInfo(out, id = ids[out][0], cluster_id = ids[out][1],
                          displayed_word = word, ts = self._now(), bad_caps = bad_caps) ]


    def _compute_count(self, wi_context):
        count = dict()

        count["clusters"] = ':'.join([ (str(c.cluster_id) if c else "*") for c in wi_context[-3:] ])
        count["ids"] = ':'.join([ (str(c.id) if c else "*") for c in wi_context[-3:] ])

        # get n-grams count
        for cluster in (False, True):

            for i in range(1, 4):
                if len(wi_context) < i: break  # only try smaller n-grams

                if cluster and not wi_context[-1].cluster_id: break  # words with no cluster associated: do not count cluster-grams

                rev_wi_context = list(reversed(wi_context))[0:i]
                ids = [ c.cluster_id if cluster else c.id for c in rev_wi_context ]
                ids.extend( [ -1 ] * (3 - i) )  # pad key to 3 elements

                num = self._db.get_gram(ids)
                if not num: break
                den = self._db.get_gram([ -4 if cluster else -2 ] + ids[1:])  # CTOTAL or #TOTAL
                if not den: break

                # get predict DB data
                (stock_count, user_count, user_replace, last_time) = num
                (total_stock_count, total_user_count, dummy1, dummy2) = den

                if not stock_count and not user_count: break

                score_id = ("c" if cluster else "s") + str(i)
                count[score_id] = Count(stock_count, user_count,
                                        total_stock_count, total_user_count,
                                        user_replace, last_time)

            if not cluster and "s1" not in count: break  # unknown word has no cluster :-)

        # compute P(wi|Ci)
        if "s1" in count and "c1" in count:
            word_count = count["s1"].count
            cluster_count = count["c1"].count
            if cluster_count:
                count["coef_wc"] = 1.0 * word_count / cluster_count


        count.pop("c1", None)  # c1 counts are useless

        return count


    def _add_wi_context(self, word, wi_context, get_count = False, create = False, uncapitalized = False):
        # this method updates its second argument (wi_context)
        first = bool(wi_context and wi_context[-1].word == '#START')

        key = '%s:%d.%d' % (word, int(uncapitalized), int(first))

        if key in self._cache:
            wi_list = self._cache[key]
        else:
            wi_list = self._word2wi(word, first_word = first, create = create, uncapitalized = uncapitalized)
            self._cache[key] = wi_list

        if not wi_list:  # unknown word -> reset context
            wi_context[:] = []
            return

        for wi in wi_list:
            wi_context.append(wi)
            if get_count:
                key2 = ':'.join( [ x.word for x in wi_context[-3:] ] )
                if key2 in self._cache:
                    wi.count = self._cache[key2]
                else:
                    wi.count = self._cache[key2] = self._compute_count(wi_context)

        return wi_list

    def _remove_bad_caps_candidates(self, candidates):
        tmp = dict()

        for c_id, c in candidates.items():
            bad = 1 if len([ 1 for wi in c if wi.bad_caps]) else 0
            key = ' '.join([ wi.word for wi in c ]).lower()
            if key not in tmp: tmp[key] = dict()
            if bad not in tmp[key]: tmp[key][bad] = set()
            tmp[key][bad].add(c_id)

        for key in tmp.keys():
            if tmp[key].get(0, None) and tmp[key].get(1, None):
                # good and bad candidates for these words --> remove the bad ones
                for c_id in tmp[key][1]:
                    del candidates[c_id]

        return candidates

    def _get_words(self, candidates, context = None, get_count = False):
        """ convert candidates (single or lists of words) to a list of WordInfo instances
            input: dict: candidate id => word or list of words
            output: dict: candidate id => WordInfo list
            (there may be more instances than input words, e.g. because of word splitting to handle
            compound words """

        wi_context = list()
        if context:
            for w in context[-3:]:
                self._add_wi_context(w, wi_context)
            self.debug("context", context, ":", " + ".join([ str(x) for x in wi_context ]))

        result = dict()
        for c_id, c in candidates.items():
            if not isinstance(c, (list, tuple)): c = [ c ]

            wi_context_candidate = list(wi_context)  # clone list
            wi_list_candidate = list()
            for w in c:
                wi_list = self._add_wi_context(w, wi_context_candidate, get_count = get_count,
                                               uncapitalized = True)  # <- candidates are uncapitalized words from curve plugin dictionary
                if not wi_list:
                    wi_list_candidate = []
                    break
                wi_list_candidate.extend(wi_list)

            # self.debug("candidates", c_id, ":", " + ".join([ str(x) for x in wi_list_candidate ]))

            result[c_id] = wi_list_candidate

        return self._remove_bad_caps_candidates(result)


    def _user_info_wi_list(self, wi_list, score_id):
        user_count = user_replace = last_time = 0
        proba = 1
        last_time = 0
        for wi in wi_list:
            if score_id not in wi.count: return None
            c = wi.count[score_id]
            if not c.total_user_count: return None  # @todo why is this happening ?
            if not user_count or c.user_count < user_count: user_count =  c.user_count
            if not user_replace or c.user_replace < user_replace: user_replace =  c.user_replace
            proba *= c.user_count / c.total_user_count
            last_time += c.last_time

        proba = proba ** (1 / len(wi_list))
        last_time /= len(wi_list)

        return (user_count, user_replace, last_time, proba)


    ALL_SCORES = [ "s3", "c3", "s2", "c2", "s1" ]

    def _compare_coefs(self, coefs1, curve_score1, coefs2, curve_score2):
        """ Compare coefficients for two words or parts of text
            Input: dicts score_id => score + curve score (for each word)
            Result:
            <= -1 = first word wins
            >= 1  = second word wins
            x (in ]-1, 1[)  = ex-aequo (x is positive if second word is slightly better) """

        curve_max_gap2 = self.cf("p2_curve_max2", 0.02, float)

        # filter candidates with bad curve (coarse setting)
        if curve_score1 > curve_score2 + curve_max_gap2: return -1
        if curve_score2 > curve_score1 + curve_max_gap2: return 1

        if not coefs1 and not coefs2: return 0
        if not coefs1: return 1
        if not coefs2: return -1


        curve_max_gap = self.cf("p2_curve_max", 0.009, float)
        curve_ratio = self.cf("p2_curve_ratio", 0.168, float)
        ratio = self.cf("p2_ratio", 100.0, float)
        fishout = self.cf("p2_fishout", 1.4, float)

        # score evaluation: linear combination of log probability ratios
        curve_coef = 10. ** (curve_ratio * (curve_score2 - curve_score1) / curve_max_gap)  # [0.1, 10], <1 if first word wins

        score_ratio = dict()
        for s_id in LanguageModel.ALL_SCORES:
            c1, c2 = coefs1.get(s_id, 0), coefs2.get(s_id, 0)

            if c1 == 0 and c2 == 0: s = 0
            elif c1 == 0: s = 1
            elif c2 == 0: s = -1
            else: s = math.log10(curve_coef * c2 / c1) / math.log10(ratio)

            score_ratio[s_id] = s

        lst = [ (1, score_ratio["s1"]),
                (self.cf("p2_coef_s2", 1, float), score_ratio["s2"]),
                (self.cf("p2_coef_s3", 1, float), score_ratio["s3"]),
                (self.cf("p2_coef_c2", 1, float), score_ratio["c2"]),
                (self.cf("p2_coef_c3", 1, float), score_ratio["c3"]) ]

        total = total_coef = 0.0
        for (coef, sc) in lst:
            if sc:
                total += coef * sc
                total_coef += coef

        score = self.cf("p2_master_coef", 2, float) * total / total_coef if total_coef else 0

        # filter candidates with bad curve (fine setting but with exception for obvious winners)
        if curve_score1 > curve_score2 + curve_max_gap and score < fishout: return -1
        if curve_score2 > curve_score1 + curve_max_gap and score > -fishout: return 1

        return score


    def _eval_score(self, candidates, verbose = False, expected_test = None):
        """ Evaluate overall score (curve + predict)
            input: candidates as a dict: candidate id => (curve score list, wordinfo list)
            ouput: score as a dict: candidate id => overall score """

        if not candidates: return dict()

        current_day = int(self._now() / 86400)

        # --- data preparation ---
        curve_scores = dict()
        max_coef = dict()
        all_coefs = dict()
        for c in candidates:
            wi_list = candidates[c][1]
            curve_scores[c] = sum(candidates[c][0]) / len(candidates[c][0])  # average

            if not wi_list: continue
            all_coefs[c] = dict()
            for score_id in LanguageModel.ALL_SCORES:
                coef = 1.0
                for wi in wi_list:
                    if wi.count and score_id in wi.count and wi.count[score_id].count > 0:
                        coef *= ((wi.count["coef_wc"] if score_id[0] == 'c' else 1.0) *
                                 wi.count[score_id].count / wi.count[score_id].total_count)
                    else: coef = 0
                coef = coef ** (1 / len(wi_list))
                if coef > 0:
                    if score_id not in max_coef or max_coef[score_id][0] < coef:
                        max_coef[score_id] = (coef, c)
                all_coefs[c][score_id] = coef

        # --- prediction score evaluation (stock) ---
        predict_scores = dict()
        score_f = lambda x: curve_scores.get(x, 0) + predict_scores[x][0] if x in predict_scores else 0

        # remove candidate with "coarse comparison failed" and keep others in a "green-list"
        green_list = set()
        for c in all_coefs.keys():
            if not green_list:
                green_list.add(c)
                continue

            remove = False
            for c0 in list(green_list):
                compare = self._compare_coefs(all_coefs[c0], curve_scores.get(c0, 0),
                                              all_coefs[c], curve_scores.get(c, 0))

                if compare >= 1:  # new candidate evict another candidate from reference list
                    green_list.remove(c0)
                elif compare <= -1:  # new candidate is eliminated by another one in reference list
                    remove = True

            if not remove: green_list.add(c)

        # ordering fine tuning for candidates still in the green-list
        last_c0 = None
        green_list = list(green_list)
        adj_f = lambda x: -0.0001 if x[0].isupper() else 0
        if green_list:
            for _ in range(2):
                green_list.sort(key = lambda x:  (score_f(x), x), reverse = True)  # the tuple ensures repeatable results
                p2_score_fine = self.cf("p2_score_fine", 0.005, float)
                self.debug("green list", *green_list)
                c0 = green_list[0]
                if c0 == last_c0: break
                last_c0 = c0

                predict_scores[c0] = (adj_f(c0), "reference")
                for c in green_list[1:]:
                    compare = self._compare_coefs(all_coefs[c0], curve_scores.get(c0, 0),
                                                  all_coefs[c], curve_scores.get(c, 0))

                    if compare:
                        predict_scores[c] = (compare * p2_score_fine + adj_f(c),
                                             "fine:%.4f" % compare)
                    else:
                        predict_scores[c] = (adj_f(c), "ex-aequo")

        for c in candidates:
            if c not in all_coefs:
                predict_scores[c] = (- self.cf("p2_score_unknown", 0.02, float), "unknown")
            elif c not in predict_scores:
                predict_scores[c] = (- self.cf("p2_score_coarse", 0.015, float), "coarse")

        # --- prediction score evaluation (user) ---
        for c in candidates:
            first = True
            for s_id in [ "s3", "s2", "s1" ]:
                wi_list = candidates[c][1]
                if not wi_list: continue

                user_info = self._user_info_wi_list(wi_list, s_id)
                if not user_info: continue
                (user_count, user_replace, last_time, proba) = user_info

                coef_age = math.exp(-0.7 * (current_day - last_time) / float(self._half_life))
                user_count *= coef_age
                user_replace *= coef_age

                if first and user_replace > user_count and user_replace > 0.5:
                    # negative learning
                    predict_scores[c] = (- self.cf("p2_learn_negative", 0.1, float), "learn-")
                    break

                if current_day - last_time > self.cf("p2_forget", 90, int): continue
                if user_count < 0.5: continue

                # positive learning
                learn_value = -1
                if s_id == "s1":
                    if user_count > self.cf("p2_learn_s1_threshold", 5, int): learn_value = 0
                    else: learn_value = - self.cf("p2_learn_s1", 0.002, float)
                else:
                    if user_count > self.cf("p2_learn_s23_threshold", 5, int) \
                       and user_replace * self.cf("p2_learn_s23_ratio", 5, int) < user_count:
                        learn_value = self.cf("p2_learn_s23", 0.01, float)
                    else: learn_value = 0

                predict_scores[c] = (max(predict_scores[c][0], learn_value), "L-%s:%.4f" % (s_id, learn_value))

                break  # simple backoff

        scores = dict()
        result = dict()
        for c in candidates:
            scores[c] = curve_scores.get(c, 0) + predict_scores[c][0]
            result[c] = dict(score = scores[c],
                             curve = candidates[c][0],
                             curve_avg = curve_scores.get(c, 0),
                             predict = predict_scores[c][0],
                             predict_reason = predict_scores[c][1],
                             stats = candidates[c][1],
                             coefs = all_coefs.get(c, dict()))


        # --- verbose output (no actual processing done here) ---
        if verbose:  # detailed display (for debug)
            # this is really fugly formatting code (i was too lazy to build tabulate or other on the phone)
            # @TODO put this in a separate function if we do not need information from above scoring
            clist = sorted(candidates.keys(),
                           key = lambda c: scores.get(c, 0),
                           reverse = True)
            txtout = []
            for c in clist:
                (candidate_curve_score, wi_list) = candidates[c]
                head = "- %5.3f [%12s] " % (scores[c], predict_scores[c][1][:12])
                if expected_test:
                    head += "*" if expected_test == c else " "
                head += "%10s " % c[0:10]

                col2 = [ (" %5.3f " % x) for x in candidate_curve_score ]
                col3 = [ ]

                for wi in wi_list or []:
                    li = "  "
                    # stock counts
                    for score_id in LanguageModel.ALL_SCORES:
                        if score_id in wi.count and wi.count[score_id].count > 0:
                            c = wi.count[score_id]
                            txt = score_id + ": " + smallify_number(c.count) \
                                + '/' + smallify_number(c.total_count) + "  "
                        else:
                            txt = " " * 15
                        li += txt

                    if "coef_wc" in wi.count:
                        li += " C=%.3f" % wi.count["coef_wc"]
                    if "clusters" in wi.count:
                        li += " [%s]" % (wi.count["clusters"])

                    col3.append(li)

                    # user counts
                    found = False
                    li = "U>"
                    for score_id in LanguageModel.ALL_SCORES:
                        if score_id in wi.count and wi.count[score_id].user_count > 0:
                            c = wi.count[score_id]
                            txt = score_id + ":" + smallify_number_3(c.user_count) \
                                + ":" + smallify_number_3(c.user_replace) \
                                + '/' + smallify_number_3(c.total_user_count) + " "
                            found = True
                        else:
                            txt = " " * 15
                        li += txt

                    if found: col3.append(li)

                while col2 or col3:
                    txtout.append(head
                                  + "|" + (col2.pop(0) if col2 else "       ")
                                  + "|" + (col3.pop(0) if col3 else ""))
                    if head[0] != ' ': head = " " * len(head)

            for l in txtout: self.log(l)

        return result


    def guess(self, context, candidates, correlation_id = -1, speed = None,
              verbose = False, expected_test = None, details_out = None):
        """ Main entry point for guessing a word
            input = candidates as a dict (word => curve score)
            ouput = ordered list (most likely word first)
            if context does not include #START tag it will be considered
            as incomplete """

        self.log("ID: %s - Speed: %d - Context: %s (reversed)" %
                 (correlation_id, speed or -1, list(reversed(context))))
        # ^^ context is reversed in logs (for compatibility with older logs)

        self.log("Candidates: " + ', '.join([ '%s[%.2f]' % (c, candidates[c])
                                              for c in sorted(candidates.keys(),
                                                              key = lambda x: candidates[x],
                                                              reverse = True) ]))

        max_score = max(list(candidates.values()) + [ 0 ])

        max_count = self.cf("p2_max_candidates", 50, int)
        if len(candidates) > max_count:
            filt = max_score - sorted(list(candidates.values()), reverse = True)[max_count]
        else:
            filt = self.cf("p2_filter", 0.025, float)

        candidates = dict([ (w, s) for w, s in candidates.items()
                            if s >= max_score - filt ])

        wi_lists = self._get_words(dict([ (x, x) for x in candidates.keys() ]), context, get_count = True)

        scores = self._eval_score(dict([ (x, ([ candidates[x] ], wi_lists.get(x, None))) for x in candidates ]),
                                  verbose = verbose, expected_test = expected_test)

        if details_out is not None: details_out.update(scores)

        result = sorted(candidates.keys(),
                        key = lambda x: scores[x]["score"] if x in scores else 0,
                        reverse = True)

        # keep guess history for later backtracking
        self._history = [ dict(result = result,
                               candidates = candidates,
                               guess = result[0] if result else None,
                               correlation_id = correlation_id,
                               expected_test = expected_test,
                               context = context) ] + self._history[:2]

        if result: self.log("Selected word: %s" % result[0])  # keep compatibility with log parser

        return result

    def backtrack(self, correlation_id = -1, verbose = False):
        """ suggest backtracking solution for last two (at the moment) guesses
            return value (tuple): new word1, new word2,
                                  expected word1 (capitalized), expected word2 (capitalized), correlation_id,
                                  capitalize 1st word (bool) """

        if len(self._history) < 2: return
        self.debug("backtrack: context[0]", self._history[0])
        self.debug("backtrack: context[1]", self._history[1])

        h0, h1 = self._history[0], self._history[1]
        if not h0["guess"] or not h1["guess"]: return

        ctx0 = h0["context"][-3:]
        ctx1 = (h1["context"] + [ h1["guess"] ])[-3:]
        self.debug("backtrack: diff", ' '.join(ctx0), ' ' .join(ctx1))
        if ' '.join(ctx0) != ' ' .join(ctx1): return

        max_count = self.cf("backtrack_max", 4, int)
        candidates = dict()
        context = h1["context"]  # @todo should be ctx1 ?

        expected_test = None
        lc0 = set(sorted(h0["candidates"], key = lambda x: h0["candidates"][x], reverse = True)[:max_count] + [ h0["guess"] ])
        lc1 = set(sorted(h1["candidates"], key = lambda x: h1["candidates"][x], reverse = True)[:max_count] + [ h1["guess"] ])
        reference_id = None

        self.log("Backtrack: ID=[%s] %s %s + %s" % (correlation_id, context, lc1, lc0))

        curve_scores = dict()
        for c0 in lc0:
            for c1 in lc1:
                id = "%s %s" % (c1, c0)
                candidates[id] = [c1, c0]

                curve_scores[id] = [ h1["candidates"].get(c1, 0), h0["candidates"].get(c0, 0) ]

                if c0 == h0["guess"] and c1 == h1["guess"]: reference_id = id

                if h1["expected_test"] and h0["expected_test"] \
                   and c1 == h1["expected_test"] and c0 == h0["expected_test"]:
                    expected_test = id

        wi_lists = self._get_words(candidates, context, get_count = True)
        if not len(wi_lists): return

        scores = self._eval_score(dict([ (x, (curve_scores[x], wi_lists.get(x, None))) for x in candidates ]),
                                  verbose = verbose, expected_test = expected_test)

        result = sorted(candidates.keys(),
                        key = lambda x: (scores[x]["score"], x) if x in scores else 0,  # repeatable result
                        reverse = True)

        guess = result[0]

        if scores[guess]["score"] > scores[reference_id]["score"] + self.cf("backtrack_threshold", 0.05, float):
            # @todo update learning
            # @todo store undo information
            w1_new = candidates[guess][0]
            w0_new = candidates[guess][1]
            w1_old = h1["guess"]
            w0_old = h0["guess"]

            self._history = []
            self.log("==> Backtracking activated: %s {%s %s}=>{%s %s}" % (context, w1_old, w0_old, w1_new, w0_new))

            return (w1_new, w0_new, w0_old, w1_old, correlation_id, None)

        return


    def cleanup(self, force_flush = False):
        """ periodic tasks: cache purge, DB flush to disc ... """
        now = int(time.time())  # no need for time mocking

        self._commit_learn(commit_all = force_flush)

        # purge
        if now > self._last_purge + self.cf("purge_every", 432000, int):
            self.log("Purge DB ...")

            current_day = int(now / 86400)
            self._db.purge(self.cf("purge_min_count1", 3, float), current_day - self.cf("purge_min_date1", 30, int))
            self._db.purge(self.cf("purge_min_count2", 20, float), current_day - self.cf("purge_min_date2", 180, int))
            self._last_purge = now
            self._db.set_param("last_purge", now)

        # save learning database
        if self._db and len(self._learn_history) == 0 and self._db.dirty:
            self.log("Flushing DB ...")
            self._db.save()  # commit changes to DB

        call_me_back = (len(self._learn_history) > 0)  # ask to be called again later

        if not call_me_back and len(self._cache) > 5000:  # purge cache
            self._cache = dict()

        return call_me_back

    def close(self):
        self.cleanup(force_flush = True)
