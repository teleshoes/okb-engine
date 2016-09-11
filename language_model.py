#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Language model for prediction engine """

import time
import re
import unicodedata
import math
import os

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
        for word in c["word_list"]:
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
        cstr = "{%s}" % (' '.join([ "%s:%s" % (x, str(y)) for x, y in self.count.items() ])) if self.count else ""
        return "[WordInfo: %s (%d:%d) %s %s %s]" % (self.word, self.id, self.cluster_id,
                                                    self.displayed_word or "-", cstr, "[BC]" if self.bad_caps else "")

    def clone(self):
        return WordInfo(self.word, self.id, self.cluster_id, self.displayed_word, self.ts, self.bad_caps)

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

        if os.getenv('LM_DEBUG', None): self._debug = True

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
            self._learn_history.pop(key, None)

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
                cachekey = ':'.join([ x.word for x in wi_context[-i:] ])
                if cachekey in self._cache:
                    type = "s"  # not learing cluster n-grams for now (for type in [ "c", "s" ]:)
                    score_id = type + str(i)
                    if score_id in self._cache[cachekey]:
                        count_cached = self._cache[cachekey][score_id]
                        count_cached.user_count += 1
                        if action: count_cached.total_user_count += 1
                        # and do not bother with age (cache will be eventually flushed)

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
            self.log("Learn:", "add" if add else "remove", wordl, "{replaces '%s'}" % replaces if replaces else "")

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

        # self.debug("_compute_count", ":".join([ w.word for w in wi_context[-3:] ]),
        #            " + ".join([ str(x) for x in wi_context ]),
        #            "->", ' '.join([ "%s:%s" % (x, str(y)) for x,y in count.items() ]))

        return count


    def _add_wi_context(self, word, wi_context, get_count = False, create = False, uncapitalized = False):
        # this method updates its second argument (wi_context)
        first = bool(wi_context and wi_context[-1].word == '#START')

        key = '%s:%d.%d' % (word, int(uncapitalized), int(first))

        if key in self._cache:
            wi_list = self._cache[key]
            wi_list = [ x.clone() for x in wi_list ] if wi_list else None
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
                    wi.count = self._compute_count(wi_context)
                    self._cache[key2] = wi.count

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

    def _get_words(self, candidates, context = None, get_count = False, uncapitalized = True):
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
                                               uncapitalized = uncapitalized)
                #                              ^^^ candidates are uncapitalized words from curve plugin dictionary
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
    ALL_SCORES_NC = [ s for s in ALL_SCORES if s[0] == "s" ]

    def _compare_coefs(self, coefs1, curve_score1, coefs2, curve_score2):
        """ Compare coefficients for two words or parts of text
            Input: dicts score_id => score + curve score (for each word)
            Result:
            <= -1 = first word wins
            >= 1  = second word wins
            x (in ]-1, 1[)  = ex-aequo (x is positive if second word is slightly better) """

        curve_max_gap2 = self.cf("p2_curve_max2", 0.011, float)

        # filter candidates with bad curve (coarse setting)
        if curve_score1 > curve_score2 + curve_max_gap2: return -1, "curve"
        if curve_score2 > curve_score1 + curve_max_gap2: return 1, "curve"

        if not coefs1 and not coefs2: return 0, "none"
        if not coefs1: return 1, "no coef 1"
        if not coefs2: return -1, "no coef 2"


        curve_max_gap = self.cf("p2_curve_max", 0.002, float)
        curve_ratio = self.cf("p2_curve_ratio", 0.7, float)
        ratio = self.cf("p2_ratio", 112.8, float)
        default = self.cf("p2_default", 1.397, float)

        # score evaluation: linear combination of log probability ratios
        if not curve_max_gap: return 0, "parms_max_gap"  # optimizer dirty workaround
        curve_coef = 10. ** (curve_ratio * (curve_score2 - curve_score1) / curve_max_gap)  # [0.1, 10], <1 if first word wins

        score_ratio = dict()
        for s_id in LanguageModel.ALL_SCORES:
            c1, c2 = coefs1.get(s_id, 0), coefs2.get(s_id, 0)

            if c1 == 0 and c2 == 0: s = 0
            elif c1 == 0: s = default
            elif c2 == 0: s = -default
            else: s = math.log10(curve_coef * c2 / c1) / math.log10(ratio)

            score_ratio[s_id] = s

        lst = [ (1, score_ratio["s1"]),
                (self.cf("p2_coef_s2", 2.256, float), score_ratio["s2"]),
                (self.cf("p2_coef_s3", 3.931, float), score_ratio["s3"]),
                (self.cf("p2_coef_c2", 2.064, float), score_ratio["c2"]),
                (self.cf("p2_coef_c3", 3.463, float), score_ratio["c3"]) ]

        total = total_coef = 0.0
        for (coef, sc) in lst:
            if sc:
                total += coef * sc
                total_coef += coef

        score = self.cf("p2_master_coef", 1.939, float) * total / total_coef if total_coef else 0

        return score, "std:%.3f %s" % (score,
                                       ", ".join([ "%s=%.2e" % (cn, cs)
                                                   for (cn, cs) in score_ratio.items() ]) if self._debug else "-")

    def _compare_coefs_debug(self, coefs1, curve_score1, coefs2, curve_score2, word1, word2):
        result = self._compare_coefs(coefs1, curve_score1, coefs2, curve_score2)
        if not self._debug: return result[0]
        tr = lambda coefs: ", ".join([ "%s=%.5f" % (x, y) for (x, y) in coefs.items() ])
        self.debug("%s[%.3f : %s] <=> %s[%.3f : %s] : compare=%.3f [%s]" %
                   (word1, curve_score1, tr(coefs1), word2, curve_score2, tr(coefs2), result[0], result[1]))
        return result[0]

    def _eval_score(self, candidates, verbose = False, expected_test = None):
        """ Evaluate overall score (curve + predict)
            input: candidates as a dict: candidate id => (curve score list, wordinfo list)
            ouput: score as a dict: candidate id => overall score """

        if not candidates: return dict()

        p2_default_coef = 10 ** (- 10 * self.cf("p2_default_coef_log", 1., float))
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
                found = False
                for wi in wi_list:
                    if wi.count and score_id in wi.count and wi.count[score_id].count > 0:
                        coef1 = ((wi.count["coef_wc"] if score_id[0] == 'c' else 1.0) *
                                  wi.count[score_id].count / wi.count[score_id].total_count)
                        found = True
                    else: coef1 = p2_default_coef  # arbitrary (small) value
                    coef *= coef1
                if not found: coef = 0
                if coef > 0:
                    coef = coef ** (1 / len(wi_list))
                    if score_id not in max_coef or max_coef[score_id][0] < coef:
                        max_coef[score_id] = (coef, c)
                all_coefs[c][score_id] = coef

        # --- prediction score evaluation (stock) ---
        predict_scores = dict()
        score_f = lambda x: curve_scores.get(x, 0) + predict_scores[x][0] if x in predict_scores else 0

        # remove candidate with "coarse comparison failed" and keep others in a "green-list"
        green_list = set()
        for c in sorted(all_coefs.keys()):  # sort for repeatable results
            if not green_list:
                green_list.add(c)
                self.debug("green list += %s (first)" % c)
                continue

            remove = False
            for c0 in list(green_list):
                compare = self._compare_coefs_debug(all_coefs[c0], curve_scores.get(c0, 0),
                                                    all_coefs[c], curve_scores.get(c, 0), c0, c)

                if compare >= 1:  # new candidate evict another candidate from reference list
                    green_list.remove(c0)
                    self.debug("green list -= %s (%.3f)" % (c0, compare))

                elif compare <= -1:  # new candidate is eliminated by another one in reference list
                    remove = True

            if not remove:
                green_list.add(c)
                self.debug("green list += %s" % c)


        # ordering fine tuning for candidates still in the green-list
        last_c0 = None
        green_list = list(green_list)
        adj_f = lambda x: -0.0001 if x[0].isupper() else 0
        p2_fine_threshold = self.cf("p2_fine_threshold", 0.288, float)
        if green_list:
            for _ in range(3):
                green_list.sort(key = lambda x:  (score_f(x), x), reverse = True)  # the tuple ensures repeatable results
                p2_score_fine = self.cf("p2_score_fine", 0.002, float)
                self.debug("green list", str(green_list))
                c0 = green_list[0]
                if c0 == last_c0: break
                last_c0 = c0

                predict_scores[c0] = (adj_f(c0), "reference")
                for c in green_list[1:]:
                    compare = self._compare_coefs_debug(all_coefs[c0], curve_scores.get(c0, 0),
                                                        all_coefs[c], curve_scores.get(c, 0), c0, c)

                    if compare:
                        if abs(compare) < p2_fine_threshold: compare = 0
                        predict_scores[c] = (compare * p2_score_fine + adj_f(c),
                                             "fine:%.4f" % compare)
                        self.debug("fine[%.4f]: '%s'" % (compare, c))
                    else:
                        predict_scores[c] = (adj_f(c), "ex-aequo")
                        self.debug("ex-aequo: '%s'" % c)

        for c in candidates:
            if c not in all_coefs:
                predict_scores[c] = (- self.cf("p2_score_unknown", 0.008, float), "unknown")
                self.debug("unknown: '%s'" % c)
            elif c not in predict_scores:
                compare = self._compare_coefs_debug(all_coefs[c0], curve_scores.get(c0, 0),
                                                    all_coefs[c], curve_scores.get(c, 0), c0, c)
                predict_scores[c] = (compare * self.cf("p2_score_coarse", 0.015, float), "coarse:%.2f" % compare)
                self.debug("coarse[%.4f]: '%s'" % (compare, c))

        # --- prediction score evaluation (user) ---
        user_stats = dict()
        max_score_id = ""
        for c in candidates:
            for s_id in LanguageModel.ALL_SCORES_NC:
                if s_id < max_score_id: break

                wi_list = candidates[c][1]
                if not wi_list: continue

                user_info = self._user_info_wi_list(wi_list, s_id)
                if not user_info: continue
                (user_count, user_replace, last_time, proba) = user_info

                coef_age = math.exp(-0.7 * (current_day - last_time) / float(self._half_life))
                user_count *= coef_age
                user_replace *= coef_age

                if user_replace > user_count and user_replace > 0.5:
                    # negative learning
                    predict_scores[c] = (predict_scores[c][0] - self.cf("p2_learn_negative", 0.01, float), "learn-:%s" % s_id)
                    break

                if current_day - last_time > self.cf("p2_forget", 90, int): continue
                if user_count < 0.5: continue

                if s_id > max_score_id:
                    max_score_id = s_id
                    user_stats = dict()

                user_stats[c] = (user_count, user_replace)

        # positive learning
        def user_score(x):
            return user_stats[x][0] - user_stats[x][1]

        lst = sorted(user_stats.keys(), key = user_score, reverse = True)
        if lst:
            max_user_score = user_score(lst[0])
            for c in user_stats:
                (user_count, user_replace) = user_stats[c]
                if max_score_id == "s1":
                    if user_count > self.cf("p2_learn_s1_threshold", 5, int): learn_value = 0
                    else: learn_value = - self.cf("p2_learn_s1", 0.002, float)
                else:
                    if user_count > self.cf("p2_learn_s23_threshold", 5, int) \
                       and user_replace * self.cf("p2_learn_s23_ratio", 5, int) < user_count \
                       and user_score(c) > max_user_score / self.cf("p2_learn_score_ratio", 4, int):
                        learn_value = self.cf("p2_learn_s23", 0.01, float)
                    else: learn_value = 0

                predict_scores[c] = (max(predict_scores[c][0], learn_value), "L+%s:%.4f" % (s_id, learn_value))

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
                    li = " %-8s" % wi.word[:8]
                    # stock counts
                    for score_id in LanguageModel.ALL_SCORES:
                        if score_id in wi.count and wi.count[score_id].count > 0:
                            cc = wi.count[score_id]
                            txt = score_id + ": " + smallify_number(cc.count) \
                                + '/' + smallify_number(cc.total_count) + "  "
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
                            cc = wi.count[score_id]
                            txt = score_id + ":" + smallify_number_3(cc.user_count) \
                                + ":" + smallify_number_3(cc.user_replace) \
                                + '/' + smallify_number_3(cc.total_user_count) + " "
                            found = True
                        else:
                            txt = " " * 15
                        li += txt

                    if found: col3.append(li)

                if c in all_coefs:
                    li = " ------- "
                    for score_id in LanguageModel.ALL_SCORES:
                        cc = all_coefs[c].get(score_id, None)
                        if cc: li += "%s: [%.2e] " % (score_id, cc)
                        else: li += " " * 15
                    col3.append(li)

                if not col3: col3.append(" (unknown)")


                while col2 or col3:
                    txtout.append(head
                                  + "|" + (col2.pop(0) if col2 else "       ")
                                  + "|" + (col3.pop(0) if col3 else ""))
                    if head[0] != ' ': head = " " * len(head)

            for l in txtout: self.log(l)

        return result


    def guess(self, context, candidates, correlation_id = -1, speed = None,
              verbose = False, expected_test = None, details_out = None,
              uncapitalized = True, history = True):
        """ Main entry point for guessing a word
            input:
             - candidates as any of the following as a dict
                 key (string) = word or list of word as a whitespace separated list
                 value (float or list of floats) = curve matching score(s)
             - context as a list of string
             - uncapitalized (bool)
                 True if words are supposed to be properly capitalized
                      (e.g. words read from surrounding text)
                 False if words are uncapitalized
                       (e.g. candidates from curve matching plugin)
            output:
             - ordered list (most likely word first)
             - updated detail_out dict if provided (with all calculation details)
            if context does not include #START tag it will be considered
            as incomplete """

        self.log("ID: %s - Speed: %d - Context: %s (reversed) - DB: %s" %
                 (correlation_id, speed or -1, list(reversed(context)),
                  self._db.dbfile))
        # ^^ context is reversed in logs (for compatibility with older logs)

        if not candidates:
            self.log("no candidate")
            return []

        candidates_tmp = dict()
        for words, score_curve in candidates.items():
            if isinstance(score_curve, (list, tuple)):
                score_curve = sum(score_curve) / len(score_curve) if score_curve else 1
            word_list = [ w for w in re.split(r'[^\w\'\-]+', words) if w ]
            candidates_tmp[words] = (word_list, score_curve)

        self.log("Candidates: " + ', '.join([ '%s[%.2f]' % (c, candidates_tmp[c][1])
                                              for c in sorted(candidates_tmp.keys(),
                                                              key = lambda x: candidates[x],
                                                              reverse = True) ]))

        sorted_candidates = sorted(candidates_tmp.keys(), key = lambda x: candidates_tmp[x][1], reverse = True)

        max_score = candidates_tmp[sorted_candidates[0]][1]
        max_count = self.cf("p2_max_candidates", 50, int)
        if len(sorted_candidates) > max_count:
            filt = max_score - candidates_tmp[sorted_candidates[max_count]][1]
        else:
            filt = self.cf("p2_filter", 0.025, float)

        candidates = dict([ (w, v) for w, v in candidates_tmp.items()
                            if v[1] >= max_score - filt ])

        wi_lists = self._get_words(dict([ (x, candidates[x][0]) for x in candidates.keys() ]), context,
                                   get_count = True, uncapitalized = uncapitalized)

        scores = self._eval_score(dict([ (x, ([ candidates[x][1] ], wi_lists.get(x, None))) for x in candidates ]),
                                  verbose = verbose, expected_test = expected_test)

        if details_out is not None: details_out.update(scores)

        result = sorted(candidates.keys(),
                        key = lambda x: scores[x]["score"] if x in scores else 0,
                        reverse = True)

        # keep guess history for later backtracking
        if history:
            self._history = [ dict(result = list(result),
                                   candidates = candidates,
                                   guess = result[0] if result else None,
                                   correlation_id = correlation_id,
                                   expected_test = expected_test,
                                   context = context,
                                   scores = scores,
                                   ts = time.time()) ] + self._history[:5]

        if result: self.log("Selected word: %s" % result[0])  # keep compatibility with log parser

        return result

    def backtrack(self, context, correlation_id = -1, verbose = False, testing = False):
        """ suggest backtracking solution for last two (at the moment) guesses
            return value (tuple): (start position, old content, new content, correlation_id) """

        timeout = self.cf("backtrack_timeout", 3, int)
        max_count = self.cf("backtrack_max", 5, int)
        score_threshold = self.cf("backtrack_score_threshold", 0.01, float)

        min_s3 = 10 ** (- 10 * self.cf("backtrack_min_s3_log", 0.6, float))

        words = []
        pattern = re.compile("[\w\'\-]+")
        for match in pattern.finditer(context):
            words.append((match.start(), match.group()))

        pos = 0
        now = time.time()
        while pos < len(words) and pos < 4:
            # @TODO handle non-swiped words
            if words[-1 - pos][1].lower() != self._history[pos]["guess"].lower(): break
            if self._history[pos]["ts"] < now - 1 - pos * timeout and not testing: break  # ignore timing aspect in replay

            if testing:
                # ignore correction done by user at recording time (allow to test more cases)
                if pos > 0 and self._history[pos - 1]["context"][-1].lower() not in [ x.lower() for x in self._history[pos]["result"] ]: break
            else:
                # if user has done a manual correction, backtracking will be disabled
                if pos > 0 and self._history[pos - 1]["context"][-1].lower() != self._history[pos]["guess"].lower(): break
            pos += 1

        self.log("*** Backtrack: [%s]" % context, "ID:", correlation_id, "Words:", words,
                 "History:", list(reversed([ h["guess"] for h in self._history ])), "matches:", pos)

        if pos < 2: return

        result = None
        result_score = 0

        index = 0
        while index < pos - 1 and index < 3:
            index += 1
            h = self._history[index]
            candidate_words = h["result"][:max_count]
            if len(candidate_words) < 2: continue

            candidates = []
            expected_test = guessed = None
            for c in candidate_words:
                new_candidate = dict(words = c,
                                     guessed = (c == h["guess"]),
                                     expected_test = (c == h["expected_test"]),
                                     score_curve = [ h["candidates"][c][1] ])
                if new_candidate["expected_test"]: expected_test = new_candidate
                if new_candidate["guessed"]: guessed = new_candidate
                candidates.append(new_candidate)

            if not guessed: continue

            index1 = index - 1
            while index1 >= 0:
                h1 = self._history[index1]
                for c in candidates:
                    c["words"] += " " + h1["guess"]
                    c["score_curve"].append(h1["candidates"][h1["guess"]][1])
                index1 -= 1

            candidates = dict([ (c["words"], c["score_curve"]) for c in candidates ])
            details = dict()
            btguess = self.guess(self._history[index]["context"], candidates, correlation_id = correlation_id,
                                 verbose = verbose, expected_test = expected_test, uncapitalized = True,
                                 details_out = details, history = False)
            if not btguess: continue

            wordguess = btguess[0].split(" ")[0]

            # do not replace with a word without S3 n-gram
            s3 = details[btguess[0]]["coefs"].get("s3", 0)
            if s3 < min_s3:
                self.debug("s3 too small (%f / %f)" % (s3, min_s3))
                continue

            score = details[btguess[0]]["score"] - details[guessed["words"]]["score"]
            ok = ""
            if score >= score_threshold and score > result_score:
                result_score = score
                result = (words[-1 - index][0] if not testing else index,  # start pos
                          self._history[index]["guess"],  # old content
                          wordguess,  # new content
                          correlation_id)
                ok = " *OK*"
            self.log("--> Backtracking candidate: Guessed=[%s] - Backtrack=[%s] - Expected=[%s] - Score=%.3f%s" %
                     (guessed["words"], btguess[0], expected_test["words"] if expected_test else 'None',
                      score, ok))

        # @todo learning update

        if result: self.log("*Backtracking success* return value = %s" % str(result))
        self.log("")

        return result


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
