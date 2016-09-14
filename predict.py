#! /usr/bin/python3
# -*- coding: utf-8 -*-

""" Simple prediction algorithm based on N-grams and word clustering """

import os
import time
import re
import language_model, backend

class Predict:
    def __init__(self, tools = None, cfvalues = dict()):
        self.predict_list = [ ]
        self.tools = tools
        self.cfvalues = cfvalues
        self.db = None
        self.lm = None

        self.preedit = ""
        self.surrounding_text = ""
        self.last_surrounding_text = ""
        self.cursor_pos = -1
        self.last_words = []
        self.sentence_pos = -1
        self.curve_learn_info_hist = [ ]
        self._mock_time = None
        self.first_word = None
        self.verbose = True  # @TODO only when logs are on

        self.debug_surrounding = os.environ.get('OKB_DEBUG_SURROUNDING', None)

    def mock_time(self, time):
        if self.lm: self.lm.mock_time(time)
        self._mock_time = time

    def _now(self):
        return self._mock_time or int(time.time())

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
        if self.lm: self.lm.close()
        if self.db: self.db.close()
        self.db = self.lm = None

    def load_db(self, force_reload = False):
        """ load database if needed """
        if self.db and not force_reload: return True

        if not os.path.isfile(self.dbfile):
            self.log("DB not found:", self.dbfile)
            return False

        self.db = backend.FslmCdbBackend(self.dbfile)
        self.lm = language_model.LanguageModel(db = self.db, tools = self.tools)
        self.log("DB open OK:", self.dbfile)

        self.lm.mock_time(self._mock_time)

        return True

    def save_db(self):
        """ save database immediately """
        if self.db: self.db.save()

    def refresh_db(self):
        pass  # no more used

    def update_preedit(self, preedit):
        """ update own copy of preedit (from maliit) """
        self.preedit = preedit
        if self.debug_surrounding: self.log("Preedit: [%s]" % preedit)

    def _learn(self, add, word, context, replaces = None, silent = False):
        if not self.lm: return
        self.lm.learn(add, word, context, replaces, silent)

    def _get_curve_learn_info(self, word, first_word):
        """ generate information for curve matching to add a new word into user
            dictionary (or at least update statistics) """

        # we don't check in word DB and just return the word as-is (with just some basic capitalization fix)
        if first_word and word[0].isupper() and word[1:].islower():
            word = word.lower()

        # compute letter list (for curve matching plugin) - just copy-paste (ahem) from loadkb.py
        letters = language_model.word2letters(word)

        return (letters, word)

    def update_surrounding(self, text, pos):
        """ update own copy of surrounding text and cursor position """

        if text == self.surrounding_text and self.cursor_pos == pos: return  # duplicate event

        self.surrounding_text = text
        self.cursor_pos = pos

        if self.debug_surrounding: self.log("Surrounding text: %s [%d]" % (text, pos))

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

        if self.debug_surrounding: self.log("Surrounding text (replace_word %s->%s): %s" % (old, new, context))


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


    def guess(self, matches, correlation_id = -1, speed = -1, show_all = False):
        """ main entry point for predictive guess """

        if not self.lm:
            self.log("guess: DB not loaded")
            return None

        self.log('[*] ' + time.ctime())

        self._update_last_words()

        words = dict()
        display_matches = []
        for x in matches:
            for y in x[4].split(','):
                word = x[0] if y == '=' else y
                words[word] = x[1]  # curve score (the rest is obsolete info)
                display_matches.append((word, words[word]))

        guesses = self.lm.guess(list(reversed(self.last_words)), words,
                                correlation_id = correlation_id, speed = speed,
                                verbose = self.verbose)

        self.predict_list = guesses[0:self.cf('max_predict', 30, int)]

        if self.predict_list:
            word = self.predict_list.pop(0)
        else:
            word = None

        # done :-)
        return word


    def backtrack(self, correlation_id = -1):
        """ After a word guess, try to correct previous words
            This is intended to be called asynchronously after a simple guess
            (see above)
            This function returns information describing how to patch the text
            and to check if modifications are still valid (in the meantime user
            may have invalidated the new guess) """

        if not self.db: return
        if not self.cf("backtrack", 1, int): return

        context = self.surrounding_text
        if not context: return
        pos = self.cursor_pos
        context = context[:pos]
        if not context: return
        if self.preedit:
            if not re.match(r'\s', context[-1]): context += " "
            context += self.preedit

        words = []
        pattern = re.compile(r'[\w\'\-]+')
        for match in pattern.finditer(context):
            words.append((match.group(), match.start()))
        words.reverse()

        hist = [ h["guess"] for h in self.lm._history ]

        self.log("Backtracking check - words:", words[:4], "hist:", hist[:4])

        # due to race conditions, the preedit & surrounding_text may not be up-to-date
        # (i.e. may have missed the last typed word)
        if (words[0][0].lower() == hist[0].lower() or words[0][0].lower() == hist[1].lower()) and \
           (words[1][0].lower() == hist[1].lower() or words[1][0].lower() == hist[2].lower()):
            btresult = self.lm.backtrack(correlation_id = correlation_id, verbose = self.verbose)
            if btresult and btresult[0] > 0:
                (index, old, new, _) = btresult
                for i in [ index - 1, index ]:
                    if words[i][0].lower() == old.lower():
                        ret = [ words[i][1], old, new, correlation_id ]
                        return ret

    def cleanup(self, force_flush = False):
        """ periodic tasks: cache purge, DB flush to disc ... """

        if not self.lm: return False
        call_me_back = self.lm.cleanup(force_flush)
        return call_me_back

    def close(self):
        """ Close all resources & flush data """
        if self.lm:
            self.cleanup(force_flush = True)
            self.lm.close()
            self.db.close()
            self.db = self.lm = None

    def get_version(self):
        try:
            with open(os.path.join(os.path.dirname(__file__), "engine.version"), "r") as f:
                return f.read().strip()
        except:
            return "unknown"
