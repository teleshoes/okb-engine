OKboard engine: framework for gesture keyboards
===============================================

Description
-----------
This is a set of libraries & tools for building gesture based virtual keyboards for mobile phones (and maybe tablets?).

It currently targets Jolla phone (but it should be easy to run it on any civilized computer running QT 5 & Maliit with touchscreen or mouse).

It's made of:
* A C++ library (libcurveplugin.so) for gesture recognition that can be used both as a QML plugin or as a simple C++ library
* A Python prediction engine (predict.py) for word prediction (mostly based on N-grams). Database persistence is done with C python modules
* Lots of tools for testing, visualization, parameters tuning, generating language files ...

See okboard project for examples on using each API (QML & Python), and included "cli" command line utility (C++ API).

How does it work
----------------
_TODO_
(magic stuff inside)

Limitations
-----------
* Prediction engine is "home made" instead of using of using off-the-shelf tools (e.g. Presage)
  At the moment, our implementation is far more advanced than Presage (clustering support, space/performance optimizations ...), and i don't know about any alternative, so this is not likely to change soon
* Only gestures over letters (A-Z) are understood. E.g. you can't swipe over the "c-cedilla" key with French keyboard (I noticed this key existed very late during development)
* There is no "user hints" to indicate word features such as double letters, accents, compound words

How to build
------------
See okboard package README file for documentation about building everything at once.

An RPM .spec file is included for packaging engine and language files.

Language files source is not included in source code distribution and must be placed in db/ sub-directory (under okb-engine source directory).
You can generate them by yourself (see section below), or extract them from existing RPMs.

How to create new language files / add new languages
----------------------------------------------------
Language files include:
* `$LANG.tre`: read-only dictionary file (stored as compact tree for space & performance reasons)
* `predict-$LANG.ng`: read-only file for storing stock n-grams in a highly efficient way (read `ngrams/fslm.c` for documentation and references)
* `predict-$LANG.db`: read-write database (this is just an ugly & dumb c-struct in a file, but it is way smaller and faster than a sqlite database (as previously done)

`$LANG` is the 2-letter language ID.

All tools needed to generate language files are included in `db/` directory

Commands below should be run from a standard Linux host (not from the Sailfish SDK).

Pre-requisites:
* You need to install lbzip2, python3 development files (`python3-dev` in some distributions), QT5 development files (`qtbase5-dev` in some distributions), and aspell-$LANG. Package name are `lbzip2` `qt5-qmake`, `aspell-$LANG` on RPM distributions. `qmake` command should run qt5 version (check your PATH).
* Build requires at least 4Gb of RAM and a fast CPU. Language file generation lasts one hour and a half on a 2.8Ghz Core i7 860

Howto:
* Define `CORPUS_DIR` and `WORK_DIR` environments variable (or set them in `~/.okboard-build` a shell configuration file which gets sourced by the build script). Also add a row `export QMAKE=qmake-qt5` if on Fedora or similar distro.
* Package all your corpora files as `$CORPUS_DIR/corpus-$LANG.txt.bz2` (compressed text file). Sentences must be separated by punctuation (".") or blank lines and capitalization should be right (e.g. proper nouns has leading upper case letter). Text must be encoded as UTF-8, but all punctuation should be ASCII. Some chars are illegal eg all outside of this set [\ \!-\~\t\r\n\'\u0080-\u023F\u20AC]. This for instance means all kinds of punctuation (eg #x2026), spaces (eg #x2002, #x2009, #x2028), dashes (eg #x2013, #x2212), items (eg #x2022) and quotes (eg #x2018, #x2019, #x201c, #x201d) needs to be replaced if used in your corpus selection.
  * A sample cleaning script is included (`okb-engine/tools/clean_corpus.py`): it removes all forbidden characters and tries to map some Unicode punctuation.
* Optionally, you can override dictionaries used by:
  * creating a `$CORPUS_DIR/words-$LANG.txt` containing the list of words to accept from input corpus (UTF-8, one word per line). If you do not provide this file, aspell dictionary is used (which is known to fail horribly with Italian),
  * creating a `$CORPUS_DIR/words-predict-$LANG.txt` containing list of words to use for prediction engine (UTF-8, one word per line). If you do not provide this file, the most used words in the corpus file will be used (cf. `predict_words` setting below). This option is useful for filtering some uncommon words overrepresented in input corpus.
  * Obviously the former should be a superset of the later
* `$WORK_DIR` should point to a directory with enough space available (English + French requires 1.5 GB)
* Create a `db/lang-$LANG.cf` configuration file (use examples from other languages). Configuration options include:
  * `predict_words`: number of words used for prediction engine (only the most used words in the corpus file will be kept). This option will be ignored if you override the dictionary with `$CORPUS_DIR/words-predict-$LANG.txt`
  * `cluster_wgrams`: number of N-grams used for words. At run-time there may be more N-grams due to learning from user typing.
  * `cluster_cgrams`: number of N-grams used for word clusters (see comments in `cluster/cluster.cpp` for detailed explanation)
  * `cluster_depth`: number of clusters. Actual cluster count will be at most `2^(cluster_depth + 1)`
  * `filter_words`: words to ignore (as a single regular expression). This is for exemple used for filtering "i" from English because "i" and "I" are the same word so the engine will automatically fall back to "I". It may be more convienient to use a cleaned dictionary (cf. `$CORPUS_DIR/words-predict-$LANG.txt` file above)
* Run `db/build.sh` to generate all language files or `db/build.sh <language code>` to build just one language. Add `-r` option to rebuild everything from scratch (this removes all temporary files)

Corpus files should include different chat style. E.g. recommendation is to use formal speech (newletters, wikipedia ...) and informal style (e-mails, IRC and chat logs, movies subtitles). As they are plain text file you can just concatenate them before bzip2 compression.

As an indication of the size required, the French corpus file is 42 million words.

### How to distribute language files
You have several options for distributing language files (`$LANG.tre`, `predict-$LANG.ng`, `predict-$LANG.db`):
* Just copy them to any Jolla device in `~/.local/share/okboard/`. When you switch language on the keyboard, new files will be avalable. No need to restart the keyboard.
* Package them as RPM files: RPM should contains just gzipped language files under `/usr/share/okboard` (with .gz extension)

There is no license restriction on produced language files.

Warning: as OKBoard is not a stable product yet, language files format will change often. In that case you'll have to rebuild language files with new source version. Format version is shown at the botton of the settings application ("DB format" line).

### Included language databases (French, English & Dutch)
Text prediction database included with the distribution has been build with the above process & the following corpora:

* English: Enron (https://foundationdb.com/documentation/enron.html) + OANC (http://www.anc.org/). This is US english only
* French: http://corpora.informatik.uni-leipzig.de, http://www.loria.fr/projets/asila/corpus_en_ligne.html
* Dutch: http://corpora2.informatik.uni-leipzig.de/download.html

In addition i've added a bunch of chat & IRC logs to these (because original corpora were not good enough for conversation style writing). I won't provide original corpora files because of possible privacy issues.

French & English corpora also include content from http://www.opensubtitles.org/ (movies subtitles).
They have been downloaded from OPUS (http://opus.lingfil.uu.se).
By request of this site, here is a link to a paper describing the OPUS project:
* http://www.lrec-conf.org/proceedings/lrec2012/pdf/463_Paper.pdf - Jörg Tiedemann, 2012, Parallel Data, Tools and Interfaces in OPUS. In Proceedings of the 8th International Conference on Language Resources and Evaluation (LREC 2012)

Text cleaning has been done manually (shell one-liners), and therefore there is no reusable tool available.

API documentation
-----------------
_TODO_ this needs proper documentation :-)

### QML API (curveplugin)

Remember all processing is asynchronous

* `startCurve(int x, int y)` Start a new gesture with first point at given coordinates
* `addPoint(int x, int y)` Add a new point to the curve
* `endCurve(int id)` Notify the plugin that the drawing has ended and wait for result to be available (blocking call)
* `endCurveAsync(int id)` Notify the plugin that the drawing has ended and return immediately. When computation is done, a `matchingDone` signal will be sent with candidates list as arguments (same format as getCandidates() return value. "id" is just a correlation ID used only in logs.
* `resetCurve()` Reset all data about gesture
* `loadKeys(QVariantList list)` Load information about keyboard geometry as a list of hashmaps with keys "x", "y", "width", "height", "caption" (a single letter string)
* `loadTree(QString fileName)` Load dictionary file (this is run asynchronously to avoid blocking the GUI)
* `setLogFile(QString fileName)` Choose output file. And empty string disables logging.
* `getResultJson()` Get all results as a JSON file
* `setDebug(bool debug)` Makes logs much more verbose
* `loadParameters(QString params)` Load parameters values as a JSON string
* `getCandidates()` Return candidate list. Each item in the list is also a QVariantList with the following elements: name, score, class, star, word list (comma separated string)

### C++ API (curveplugin)
_TODO_

### Python prediction API
It is provided by the Predict class. All calls should be invoked with pyOtherSide asynchronous calls to avoid GUI freezes.

* `set_dbfile(filename)` Load given word prediction DB file
* `refresh_db()` Issue a dummy request to database (in order to speed up later real requests)
* `update_preedit()` Notify the engine of preedit changes (just hook this to "onPreeditChanged" signal)
* `update_surrounding(text, position)` Notify the engine of surrounding text changes (hook this to "onSurroundingTextChanged" signal)
* `replace(old, new)` Notify the engine that the user has replaced a word with another (e.g. with a multiple choice menu)
* `guess(candidates, correlation_id)` Returns guessed word
* `get_predict_words()` Returns alternate choices (used for prediction bar)
* `cleanup()` Run periodic tasks (learning, DB flush, cache management). This should be called during user inactivity periods. If return value is True, you should call this function again later.
* `backtrack()` [IN PROGRESS] when you type a word, it may become obvious that the previous guessed word was wrong, so this function returns information needed to correct it

TODO
----
### Short term
* [DONE] Support for learning new words: updatable .tre file or separate storage, and new C/QML plugin API.
  Now we have reduced DB size, this is most needed to allow users to add new words (jargon words, friends name ...)
* Handle really badly written words -> detect this case and have much more reliance on word prediction engine. On the contrary add more weight to a simpler algorithm for near-perfect strokes
* Form for sending detailed logs by e-mail for analysis or post-mortem
* Package as a single project
* Package as a layout (such as Dolphin keyboard in OpenRepos), not as a separate maliit plugin. This would allow to get rid of the standalone settings application and the need to restart maliit server. This would require an internal language switcher though.
* Redo score aggregation (swipe score + prediction + learning ...) based on 2015 data collection: filter word list, adjust for low n-gram counts, etc.
* Redo turn-score based on 2015 data collection (currently it has very good average results, but some failures that prevent the right word to be selected)
* [Runs but unusable due to different resolution] Jolla tablet support

### Middle term
* [Initial support done, still not really usable] Implement two-hand swiping (like Keymonk or Nintype): This is probably useful for landscape mode or new tablet
* Refactoring: Having separate gesture engine (C++ plugin with internal threading) and Python prediction engine makes some features very difficult to implement (e.g. learning new words)
  This would avoid unnecessary data & code duplication, and inefficient communication (python <-> QML <-> C++)
* Add Xt9 replacement: this would enable us to completely fork from Jolla keyboard. This would require to improve the prediction engine to handle partially typed words (and above refactoring is also a pre-requisite)
* Improve learning: "learn" usage of cluster n-grams + assign new words to existing cluster: maybe a background task that evaluate perplexity increase for each (word, cluster) values
* Better documentation :-) ... and explain the algorithm because the code is not very friendly (due to lot of trial and error)
* Allow user hints for word features: double letters, accents, middle key when 3+ keys are aligned (in the last case, user slowing down is detected but it's not always easy to perform),
  compound words separators (apostrophe, hyphen), capitalization. These should all stay optional.
* [PARTLY DONE] Better error management (fail gracefully in case of disk full, database corrupted, etc.)
* [STARTED but very crude] Auto-tune coefficients between curve matching and word prediction to adapt to user style (may be based on speed or error count). Maybe do the same with some parameters (.cf file)
* Better handling of compound words (i.e. containing hyphens or apostrophs). They should be handled as a sequence of words in the prediction engine. Maybe store go-between characters as n-grams attributes
  Today they are considered as single words, so some of them are rare in the learning corpus and so are difficult to input. Optionally the one-word approach could be kept for high count n-grams.
  No change to be done to curve matching part, we need to be able to type these words as a single swipe.
* [DONE but could be more user friendly] Tools for easily adding and packaging new languages
* Automatic non-regression tests for all the word prediction part and language file generation
* Tool for collecting user data (learning data, logs) ... with user permission.
  Provide anonymisation features when possible (remove all proper names, non words items such as number, new words learnt ...)
* Find a solution for long words (they are so boring to type)
* Suggest words during "swiping" so you can just stop drawing when you see the word you want. I'm not sure this is really practical.
* [WON'T DO] Use third party prediction engine (Presage, is there any other ?)
* Clean up int/float mess in the curve plugin
* Support different screen sizes and resolutions - this should allow landscape mode or usage on tablet
* [DONE] Compact read/write storage for word prediction database (sqlite replacement), for speed & size
* All threading should be rewritten in the QT way (see pyotherside source code) instead of home made and error-prone threads plumbing
* Add new languages [easy: now it's mostly automated, we only need good quality text corpus, and some manual tuning]
* Large scale testing campaign with lot of users to collect information on different user styles (and improve test cases)
* Check is there would be a gain if all data files are mmapped (our larger language is now 6MB so it may not matter)
* Handle letters that appears multiple times on the keyboard (With and without diacritics, such as "ç" or "å"). As this is not handled by the model, this may be a postprocessing workaround (but matching must use a list of coordinates for each key)
* Make language file build tools more memory efficient for large corpus files (and if it is not enough, use the same trick as the sort command)

### Long term / research projects
* Manually clean dictionary files (i probably don't need Enron guys name)
* Use semantic context for improving prediction. LSA (Latent Semantic Analysis) may be a good candidate, cf. this excellent paper: "Adaptive word prediction and its application in an assistive communication system" (Tonio Wandmacher) http://d-nb.info/993235727/34 (but matrix calculations may require too much CPU time on device). Also other solutions are described in this paper: "Unsupervised Methods for Language Modeling" (Tomáš Brychcín) http://www.kiv.zcu.cz/site/documents/verejne/vyzkum/publikace/technicke-zpravy/2012/tr-2012-03.pdf
* Explore other tricks found in above paper: coefficient optimization
* Variable length N-grams, at least for clusters: Existing research on POS (Part of Speech) can probably be reused (this is the same problem: we are just using automatically found clusters instead of manually tagged words). Exemple: Evaluate all N-grams, but don't go deeper when results converge -> a simple simulation could give us the required N-grams count
* Use machine learning algorithms - this will require a larger set of good quality test cases for learning (current generated test cases are not an option, so this will happen after release)
   - curve matching: building the first version gave us indications on what feature could be used (e.g. existing scores and checks): This can be seen as a pair-wise ranking problem or classification problem (try both)
   - aggregation between gesture score and prediction engine scores: also a ranking problem
* Support for non-latin languages
* API to load/unload/select context & DB (dictionary & prediction DB) --> e.g. using keyboard for specialized uses (on-device development ...)
* Improve word prediction: use real smoothing and backoff (instead of current constant linear interpolation hack)
* [as incredible as it sounds ... DONE] Improve word prediction: use word classes / groups / tags to increase result for low count N-grams. An automatic classifier would allow to easily add new languages, as existing tag databases are very sparse
* Use grammar rules to improve word prediction (I definitely won't code this)
* Handle compound words without separation signs (needed for German, but maybe it's a bad candidate for this kind of input method)
