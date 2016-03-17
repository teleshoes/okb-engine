#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Language model for prediction engine """

import time
import re

def smallify_number(number):
    if number <= 9999: return "%4d" % int(number)
    if number <= 999999: return "%3dk" % int(number / 1000)
    return "%3dm" % int(number / 1000000)

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
    def __init__(self, word, id, cluster_id, disp_word = None, ts = 0, bad_caps = False):
        self.word = word  # actual word in the database
        self.id = id
        self.cluster_id = cluster_id
        self.disp_word = disp_word  # shown word (e.g. with capitalized)
        self.ts = ts
        self.count = dict()
        self.bad_caps = bad_caps

    def __str__(self):
        return "%s [%d:%d] %s %s" % (self.word, self.id, self.cluster_id, self.disp_word or "-", self.count or "")

class Count:
    def __init__(self, *params):
        (self.count, self.user_count, self.total_count,
         self.total_user_count, self.user_replace, self.last_time) = params

    def __str__(self):
        return "[count=%d total=%d user=%d user_total=%d user_replace=%d]" % \
            (self.count, self.total_count, self.user_count, self.total_user_count, self.user_replace)


class LanguageModel:
    def __init__(self, db = None, tools = None, cfvalues = dict()):
        self._db = db
        self._tools = tools
        self._cfvalues = cfvalues
        self._mock_time = None
        self._cache = dict()
        self._learn_history = []
        self._last_purge = self._db.get_param("last_purge", 0, int)

    def _mock_time(self, time):
        self._mock_time = time

    def _now(self):
        return self._mock_time or int(time.time())

    def cf(self, key, default_value, cast = None):
        """ "mockable" configuration """
        if self._db and self._db.get_param(key):
            ret = self._db.get_param(key)  # per language DB parameter values
        elif self._tools:
            ret = self._tools.cf(key, default_value, cast)
        elif self.cf:
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


    def commit_learn(self, commit_all = False):
        pass # @TODO


    def learn(self, add, word, context, replaces = None, silent = False):
        pass # @TODO


    def replace_word(self, old, new, context = None):
        pass # @TODO


    def _word2wi(self, word, first_word = False):
        """ convert a word to a WordInfo instances list
            there may be multiple instances, e.g. because
            of word splitting to handle compound words """

        # handle compound words
        if word.find("'") > -1 or word.find("-") > -1:
            # prefixes
            mo = re.match(r'^([^\-\']+[\-\'])(.+)$', word)
            if mo:
                l1 = self._word2wi(mo.group(1), first_word)  # prefix
                l2 = self._word2wi(mo.group(2), False)
                if l1 and l2: return l1 + l2
            # suffixes
            mo = re.match(r'^(.+)([\-\'][^\-\']+)$', word)
            if mo:
                l1 = self._word2wi(mo.group(1), first_word)
                l2 = self._word2wi(mo.group(2), False)  # suffix
                if l1 and l2: return l1 + l2

        # handle full words
        ids = self._db.get_words(word)
        if not ids: return None

        bad_caps = False
        if first_word and word.lower() in ids:  # find uncapitalized word (first word only)
            out = word.lower()
        elif word in ids:  # find exact word
            out = word
        # in remaining cases, words matched have inconsistent capitalization (best effort mode)
        elif word.lower() in ids:
            out = word.lower()
            bad_caps = True
        else:
            out = list(ids.keys())[0]  # random choice
            bad_caps = True

        return [ WordInfo(out, id = ids[out][0], cluster_id = ids[out][1],
                          disp_word = word, ts = self._now(), bad_caps = bad_caps) ]


    def _compute_count(self, wi_context):
        count = dict()

        count["clusters"] = ':'.join([ (str(c.cluster_id) if c else "*") for c in wi_context[-3:] ])
        count["ids"] = ':'.join([ (str(c.id) if c else "*") for c in wi_context[-3:] ])

        # get n-grams count
        for i in range(1, 4):
            if len(wi_context) < i: continue  # only try smaller n-grams

            for cluster in (False, True):
                rev_wi_context = list(reversed(wi_context))[0:i]
                ids = [ c.cluster_id if cluster else c.id for c in rev_wi_context ]
                ids.extend( [ -1 ] * (3 - i) )  # pad key to 3 elements

                num = self._db.get_gram(ids)
                den = self._db.get_gram([ -4 if cluster else -2 ] + ids[1:])  # CSTART or #START
                if not num or not den: continue

                # get predict DB data
                (stock_count, user_count, user_replace, last_time) = num
                (total_stock_count, total_user_count, dummy1, dummy2) = den

                if not stock_count and not user_count: continue

                score_id = ("c" if cluster else "s") + str(i)
                count[score_id] = Count(stock_count, user_count,
                                        total_stock_count, total_user_count,
                                        user_replace, last_time)

        # compute P(wi|Ci)
        if "s1" in count and "c1" in count:
            word_count = count["s1"].count
            cluster_count = count["c1"].count
            if cluster_count:
                count["coef_wc"] = 1.0 * word_count / cluster_count


        count.pop("c1", None)  # c1 counts are useless

        return count


    def _add_wi_context(self, word, wi_context, get_count = False):
        first = (wi_context and wi_context[-1].word == '#START')
        key = ':'.join( [ x.word for x in wi_context[-2:] ] + [ word ])
        if key in self._cache:
            wi_list = self._cache[key]
        else:
            wi_list = self._word2wi(word, first_word = first)
            self._cache[key] = wi_list

        if not wi_list:  # unknown word -> reset context
            wi_context[:] = []
            return

        for wi in wi_list:
            wi_context.append(wi)
            if get_count and not wi.count:
                wi.count = self._compute_count(wi_context)

        return wi_list


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

        result = dict()
        for c_id, c in candidates.items():
            if not isinstance(c, (list, tuple)): c = [ c ]

            wi_context_candidate = list(wi_context)  # clone list
            wi_list_candidate = list()
            for w in c:
                wi_list = self._add_wi_context(w, wi_context_candidate, get_count = get_count)
                if not wi_list:
                    wi_list_candidate = []
                    break
                wi_list_candidate.extend(wi_list)

            result[c_id] = wi_list_candidate

        #@TODO eliminate candidates with bad caps if there are more appropriate candidates (WordInfo.bad_caps flag)

        return result


    ALL_SCORES = [ "s3", "c3", "s2", "c2", "s1" ]


    def _eval_score(self, candidates, verbose = False, expected_test = None):
        """ Evaluate overall score (curve + predict)
            input: candidates as a dict: candidate id => (curve score list, wordinfo list)
            ouput: score as a dict: candidate id => overall score """

        scores = dict()

        #@TODO implement the real formula here :-) (sounds easy)
        for c in candidates:
            # prediction engine si just a no-op
            scores[c] = sum(candidates[c][0]) / len(candidates[c][0])


        result = dict()
        for c in candidates:
            result[c] = dict(score = scores.get(c, 0),
                             curve = candidates[c][0],
                             stats = candidates[c][1])

        if verbose:  # detailed display (for debug)
            # this is really fugly formatting code (i was too lazy to build tabulate or other on the phone)
            # @TODO put this in a separate function if we do not need information from above scoring
            clist = sorted(candidates.keys(),
                           key = lambda c: scores.get(c, 0),
                           reverse = True)
            txtout = []
            for c in clist:
                (curve_scores, wi_list) = candidates[c]
                head = "- %5.3f " % scores.get(c, 0)
                if expected_test:
                    head += "*" if expected_test == c else " "
                head += "%10s " % c[0:10]

                col2 = [ (" %5.3f " % x) for x in curve_scores ]
                col3 = [ ]
                for wi in wi_list:
                    li = " "
                    # stock counts
                    for score_id in LanguageModel.ALL_SCORES:
                        if score_id in wi.count and wi.count[score_id].count > 0:
                            c = wi.count[score_id]
                            txt = score_id + ": " + smallify_number(c.count) \
                                + '/' + smallify_number(c.total_count) + " "
                        else:
                            txt = " " * 14
                        li += txt

                    if "coef_wc" in wi.count:
                        li += " C=%.3f" % wi.count["coef_wc"]
                    if "clusters" in wi.count:
                        li += " [%s]" % (wi.count["clusters"])

                    col3.append(li)

                    # user counts
                    # @TODO

                while col2 or col3:
                    txtout.append(head
                                  + "|" + (col2.pop(0) if col2 else "       ")
                                  + "|" + (col3.pop(0) if col3 else ""))
                    head = " " * len(head)

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

        wi_lists = self._get_words(dict([ (x, x) for x in candidates.keys() ]), context, get_count = True)

        scores = self._eval_score(dict([ (x, ([ candidates[x] ], wi_lists[x])) for x in candidates ]),
                                  verbose = verbose, expected_test = expected_test)

        if details_out: details_out.update(scores)

        return sorted(candidates.keys(),
                      key = lambda x: scores[x]["score"] if x in scores else 0,
                      reverse = True)


    def backtrack(self, correlation_id = -1, verbose = False, expected_test = None):
        """ suggest backtracking solution for last two (at the moment) guesses
            return value (tuple): new word1, new word2,
                                  expected word1 (capitalized), expected word2 (capitalized), correlation_id,
                                  capitalize 1st word (bool) """

        # return (found[1], found[0], h0["context"][0], h0["guess"], correlation_id, h1["context"] and h1["context"][0] == '#START')
        pass # @TODO (and update learning)


    def cleanup(self, force_flush = False):
        now = int(time.time())  # no need for time mocking

        self.commit_learn(commit_all = force_flush)

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

        return call_me_back

    def close(self):
        self.cleanup(force_flush = True)
