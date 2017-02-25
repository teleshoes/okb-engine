# this makefile implements all the steps needed to build language specific resource files.
# It only requires an input text corpus ($CORPUS_DIR/corpus-<lang>.txt.bz2)
#  and a settings file (lang-<lang>.cf) per language.
# The make switch -j is your friend if you want to build several languages at once.

TOOLS_DIR ?= ../tools
CORPUS_DIR ?= /tmp
DEST_DIR ?= .

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))

getconf = $(shell grep '^'$(2)'=' $(1) default.cf | sed 's/^.*=//' | head -n 1)

include $(patsubst lang-%.cf,.depend-%,$(wildcard lang-*.cf))

all: $(patsubst lang-%.cf,all-%,$(wildcard lang-*.cf))

all-%: predict-%.db %.tre grams-%-test.csv.bz2 wordstats-%.rpt
	@echo OK $*

depend: $(patsubst lang-%.cf,.depend-%,$(wildcard lang-*.cf))

.depend-%:
	$(eval lang=$*)
	cat $(mkfile_path) | egrep '^[a-z0-9\.\%\-]+:.*%' | sed 's/%/$(lang)/g' > .depend-$(lang)

# split input corpus into learn and test
%-learn.txt.bz2: $(CORPUS_DIR)/corpus-%.txt.bz2
	$(eval lang=$*)
	lbzip2 -d < $(CORPUS_DIR)/corpus-$(lang).txt.bz2 | $(TOOLS_DIR)/corpus-splitter.pl 1000 50 $(lang)-learn.tmp.bz2 $(lang)-test.tmp.bz2
	mv -vf $(lang)-learn.tmp.bz2 $(lang)-learn.txt.bz2
	mv -vf $(lang)-test.tmp.bz2 $(lang)-test.txt.bz2

%-test.txt.bz2: %-learn.txt.bz2

# build full dictionary (only used for gesture engine tests)
# either from provided words-$(lang).txt or aspell lexicon words
# and if provided add-words-$(lang).txt.
%-full.dict: $(wildcard $(CORPUS_DIR)/words-%.txt)
	$(eval lang=$*)
	( [ -f "add-words-$(lang).txt" ] && cat "add-words-$(lang).txt" ; \
	  if [ -f $(CORPUS_DIR)/words-$(lang).txt ] ; \
	  then cp -f $(CORPUS_DIR)/words-$(lang).txt $(lang)-full.dict.tmp ; \
	  else aspell -l $(lang) dump master | aspell -l $(lang) expand | tr ' ' '\n' ; fi) | sort | uniq > $(lang)-full.dict.tmp
	mv -f $(lang)-full.dict.tmp $@

# build full N-grams list from learn corpus
grams-%-full.csv.bz2: %-full.dict %-learn.txt.bz2
	$(eval lang=$*)
	set -o pipefail ; lbzip2 -d < $(lang)-learn.txt.bz2 | $(TOOLS_DIR)/import_corpus.py $(lang)-full.dict | sort -rn | lbzip2 -9 > grams-$(lang)-full.csv.bz2.tmp
	mv -f grams-$(lang)-full.csv.bz2.tmp grams-$(lang)-full.csv.bz2

# select words to use with prediction engine from learn corpus
%-predict.dict: grams-%-full.csv.bz2 $(wildcard $(CORPUS_DIR)/words-predict-%.txt)
	$(eval lang=$*)
	$(eval words=$(call getconf,lang-$(lang).cf,predict_words))
	$(eval words_coverage=$(call getconf,lang-$(lang).cf,predict_words_coverage))
	$(eval filter=$(call getconf,lang-$(lang).cf,filter_words))
	set -o pipefail ; lbzip2 -d < $< | grep ';#NA;#NA;' | cut -f '1,4' -d';' \
	 | grep -v '#TOTAL' | sort -rn | cut -d';' -f 2 | egrep -v '^$(filter)$$' > tmp-words-$(lang).txt

	set -o pipefail ; \
	if [ -f $(CORPUS_DIR)/words-predict-$(lang).txt ] ; then \
		echo "Using provided prediction dictionary file for language $(lang)" ; \
		cp -vf $(CORPUS_DIR)/words-predict-$(lang).txt $(lang)-predict.dict.tmp ; \
		cat $(CORPUS_DIR)/words-predict-$(lang).txt tmp-words-$(lang).txt | sort | uniq > tmp-words-$(lang).txt.tmp ; \
		mv -f tmp-words-$(lang).txt.tmp tmp-words-$(lang).txt ; \
	elif [ "$(words)" = 0 ] ; then \
		lbzip2 -d < $< | grep ';#NA;#NA;' | cut -f '1,4' -d';' \
		 | grep -v '#TOTAL' | sort -rn | $(TOOLS_DIR)/words_coverage.py $(words_coverage) > $(lang)-predict.dict.tmp ; \
	        cat $(lang)-predict.dict.tmp >> tmp-words-$(lang).txt ; \
	else \
	        cat tmp-words-$(lang).txt | sed -n "1,$(words) p" > $(lang)-predict.dict.tmp ; \
	fi # ^^^ ok i've re-implemented "head" with sed to avoid ugly sigpipes (which hurt with -o pipefail)
	mv -f $(lang)-predict.dict.tmp $@

tmp-words-%.txt: %-predict.dict

# find affixes
affixes-%.txt: %-predict.dict
	$(eval lang=$*)
	set -o pipefail ; cat $(lang)-predict.dict | $(TOOLS_DIR)/find_affixes.py 5 > affixes-$(lang).txt.tmp && mv -vf affixes-$(lang).txt.tmp affixes-$(lang).txt

# build dictionary file for gesture engine
%.tre: %-full.dict %-predict.dict
	$(eval lang=$*)
	$(TOOLS_DIR)/loadkb.py $(lang)-full.tre < $(lang)-full.dict
	$(TOOLS_DIR)/loadkb.py $@ < tmp-words-$(lang).txt  # all word seen in learn corpus (smaller than full directory, but bigger than prediction learning dictionary)

# build N-grams list for learning & test corpora
grams-%-learn.csv.bz2: %-predict.dict %-learn.txt.bz2 affixes-%.txt
	$(eval lang=$*)
	set -o pipefail	; lbzip2 -d < $(lang)-learn.txt.bz2 | $(TOOLS_DIR)/import_corpus.py -a affixes-$(lang).txt $(lang)-predict.dict | lbzip2 -9 > grams-$(lang)-learn.csv.bz2.tmp
	mv -f grams-$(lang)-learn.csv.bz2.tmp grams-$(lang)-learn.csv.bz2

grams-%-test.csv.bz2: %-predict.dict %-test.txt.bz2 affixes-%.txt
	$(eval lang=$*)
	set -o pipefail ; lbzip2 -d < $(lang)-test.txt.bz2 | $(TOOLS_DIR)/import_corpus.py -a affixes-$(lang).txt $(lang)-predict.dict | lbzip2 -9 > grams-$(lang)-test.csv.bz2.tmp
	mv -f grams-$(lang)-test.csv.bz2.tmp grams-$(lang)-test.csv.bz2

# compute optimal cluster list (a bit slow)
clusters-%.txt: grams-%-learn.csv.bz2
	$(eval lang=$*)
	$(eval cluster_filter_words=$(call getconf,lang-$(lang).cf,cluster_filter_words))
	$(eval cluster_filter=$(call getconf,lang-$(lang).cf,cluster_filter))
	@echo "Computing clusters for language $(lang). Please make some coffee ..."
	@echo " (logs can be found in clusters-$(lang).log)"
	set -o pipefail ; lbzip2 -d < $< | sort -n \
	 | $(TOOLS_DIR)/ngram_filter.py -l ngram_filter-$(lang).log $(cluster_filter_words) $(cluster_filter) \
	 | $(TOOLS_DIR)/cluster -n 10 -o clusters-$(lang).tmp > clusters-$(lang).log 2>&1
	mv -f clusters-$(lang).tmp $@

# word statistics
wordstats-%.rpt: grams-%-learn.csv.bz2
	$(eval lang=$*)
	lbzip2 -d < grams-$(lang)-learn.csv.bz2 | grep ';#NA;#NA;' | sort -rn \
	 | grep -v '#TOTAL' | cut -d';' -f 1,4 \
	 | $(TOOLS_DIR)/wordstats2.py > wordstats-$(lang).tmp
	mv -f wordstats-$(lang).tmp $@

# build prediction database
predict-%.db: clusters-%.txt grams-%-learn.csv.bz2 db.version
	$(eval lang=$*)
	$(eval depth=$(call getconf,lang-$(lang).cf,cluster_depth))
	$(eval cgrams=$(call getconf,lang-$(lang).cf,cluster_cgrams))
	$(eval wgrams=$(call getconf,lang-$(lang).cf,cluster_wgrams))

	set -o pipefail ; (lbzip2 -d < grams-$(lang)-learn.csv.bz2 ; echo "=" ; lbzip2 -d < grams-$(lang)-test.csv.bz2) \
	 | $(TOOLS_DIR)/clusterize.py -r ngrams-$(lang).rpt -l $(depth) -w $(wgrams) -c $(cgrams) \
	                              -t $(TOOLS_DIR)/threshold.cfg clusters-$(lang).txt \
	 | tee predict-$(lang).txt \
	 | $(TOOLS_DIR)/load_cdb_fslm.py predict-$(lang)-tmp.db

	lbzip2 -9fv predict-$(lang).txt

	$(eval id=$(shell date '+%s')) # DB generation ID (used to notify user of a new DB)
	$(TOOLS_DIR)/db_param.py predict-$(lang)-tmp.db id $(id)
	echo $(id) > predict-$(lang).id

	$(TOOLS_DIR)/db_param.py predict-$(lang)-tmp.db version $(shell cat db.version)
	lbzip2 -9f predict-$(lang)-tmp.rpt
	mv -f predict-$(lang)-tmp.ng predict-$(lang).ng
	mv -f predict-$(lang)-tmp.rpt.bz2 predict-$(lang).rpt.bz2
	mv -f predict-$(lang)-tmp.db $@


clean:
	rm -f *.tmp* *-{learn,test}.txt.bz2 grams-*.csv.bz2 *.tre *.dict clusters-*.{txt,log}
	rm -f predict-*.{db,ng,log,rpt,rpt.bz2} tmp-words-*.txt *.id ngrams-*.rpt wordstats-*.rpt
	rm -f ngram_filter-*.log

.PRECIOUS: affixes-%.txt
