OKboard engine: tools for gesture keyboards and word prediction
===============================================================

Description
-----------
This is a set of tools for building gesture based virtual keyboards. It targets SailfishOS devices (but it should work on any civilized computer running QT 5 with touchscreen or mouse).

It's made of:

* A C++ library (libcurveplugin.so) for gesture recognition that can be used both as QML plugin or as a simple C++ library (see below for info on APIs)
* A Python prediction engine (predict.py) for word prediction (mostly based on N-grams)
* Lots of tools for testing, visualization, parameters tuning, adding new languages (tools/ directory) ...

See OKboard project for examples on using each API (QML & Python), and included "cli" command line utility (C++ API).

How does it work
----------------
_TODO_

Limitations
-----------
* Current implementation does not support learning new words.
  As a result it is shipped with very large dictionnaries and prediction datatases (French DB can recognise 650k words)
  This was probably a very dumb design decision, but it allowed to stress test and tune the algorithm a lot.
* Prediction engine is "home made" instead of using of using off-the-shelf tools (e.g. Presage)
  A switch to Presage has not been undertaken because of performance and space optimization missing in this product.
* Prediction engine if often slow mainly due to sqlite access -> this part should be replaced (e.g. in memory storage)
* Only gestures over letters (A-Z) are understood. E.g. you can't swipe over the "c-cedilla" key with French keyboard (I noticed this key existed very late during development)

How to build
------------
qmake / make for C++ parts.

RPM .spec file is included for packaging engine and language files.

Language files source is not included in source code distribution and mush be placed under db/ sub-directory:
You can generate them by yourself (see section below), or extract them from existing RPMs.

How to create new language files / add new languages
----------------------------------------------------
Commands are to be run from a working directory. `$TOOLS` represents "tools" directory in okb-engine source directory, `$LANG` the 2 letter language code.

Language files are comprised of:

* `$LANG.tre`: dictionary file (stored as compact tree for space & performance issues)
* `predict-$LANG.db`: sqlite DB for word prediction

With current version this only works with languages with latin alphabet, and it has not been tested with any other language that French & English.

### Dictionary file
`cat <dictionary file> | $TOOLS/loadkb.py $LANG.tre`

Dictionary file is UTF-8 encoded and contains one word per line.
You can possibly generate them with aspell with `aspell <language> dump master` but you'll end with very big dictionaries (probably useless for a phone keyboard).

### Word prediction database
`cat <corpus file> | $TOOLS/import_corpus.py <dictionary file> | sort -n | tail -n <ngram count> | $TOOLS/load_sqlite.py predict-$LANG.db`

Corpus files must contains UTF-8 text, with only significant content. Sentences must be separated with periods. Line breaks are ignored.
N-gram count is a trade off between space used and accuracy.


### Included databases (French & English)
Text prediction DB included with the distribution has been build with the above process & the following corpora:

* English: Enron (https://foundationdb.com/documentation/enron.html) + OANC (http://www.anc.org/). This is US english only
* French: http://corpora.informatik.uni-leipzig.de, http://www.loria.fr/projets/asila/corpus_en_ligne.html and some stuff i haven't kept track off

Text cleaning has been done manually (shell one-liners), and therefore there is no tools available.

N-gram count has been set to 400k (completely arbitrary choice).

Dictionary files have been produced with the above aspell trick.

API documentation
-----------------
_TODO_ this needs proper documentation :-)

### QML API (curveplugin)

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

TODO
----
### Short term
* Support for learning new words: updatable .tre file or separate storage, and new C/QML plugin API.
  This would allow to start with much smaller databases (.tre and word predict) and greatly increase performance & word recognition rate.
* Handle really badly written words -> detect this case and have much more reliance on word prediction engine

### Middle term
* Better documentation :-) ... and explain the algorithm because the code is not very friendly (due to lot of trial and error)
* Code simplification: some parts are really complicated and are not that useful (all logs about score, turn score, classifier ...). 
  Possibly over one-third of the code could be headed to the trashcan.
* Allow user hints for word features: double letters, accents, middle key when 3+ keys are aligned (in the last case, user slowing down is detected but it's not always easy to perform), 
  compound words separators (apostrophe, hyphen), capitalization. These should all stay optional.
* Better error management (fail gracefully in case of disk full, database corrupted, etc.)
* Auto-tune coefficients between curve matching and word prediction to adapt to user style (may be based on speed or error count). Maybe do the same with some parameters (.cf file)
* Better handling of compound words (i.e. containing hyphens or apostrophs)
* Tools for easily adding and packaging new languages
* Automatic non-regression tests for all the word prediction part
* Tool for collecting user data (learning data, logs) ... with user permission.
  Provide anonymisation features when possible (remove all proper names, non words items such as number, new words learnt ...)
* Large scale testing campaign with lot of users to collect information on different user styles (and improve test cases)
* Find a solution for very long words (they are so boring to type)
* Use third party prediction engine (Presage, is there any other ?)
* Clean up int/float mess in the curve plugin
* Support different screen sizes and resolutions - this should make landscape mode easier
* Compact storage for word prediction database (replace sqlite), for speed & size
* All threading should be rewritten in the QT way (see pyotherside source code) instead of home made and error-prone threads plumbing
* Add new languages

### Long term / research projects
* Support for non-latin languages
* API to load/unload/select context & DB (dictionary & prediction DB) --> e.g. using keyboard for specialized uses (on-device development ...)
* Improve word prediction: use real smoothing and backoff (instead of current constant linear interpolation hack)
* Improve word prediction: use word classes / groups / tags to increase result for low count N-grams,
  e.g. see "Linearly Interpolated Hierarchical N-gram" (Imed Zitouni and Qiru Zhou) (need a proper quote)
  -> automatic classifier would allow to easily add new languages, as existing tag databases are very sparse
* Improve prediction engine to handle partially typed words (with possible errors): this could provide a replacement for Xt9 for new maliit plugins
* Use grammar rules to improve word prediction (I definitely won't code this)
