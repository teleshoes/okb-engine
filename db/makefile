# this makefile implements all the steps needed to build language files
#  input required is only a text corpus ($CORPUS_DIR/corpus-<lang>.fr)
#  and a settings file (lang-<lang>.cf)
# -j is your friend if you've got several languages

TOOLS_DIR ?= ../tools
CORPUS_DIR ?= /tmp
DEST_DIR ?= .

getconf = $(shell grep '^'$(2)'=' $(1) | sed 's/^.*=//' | head -n 1)

.SECONDARY:

all-%: predict-%.db
	true

%-learn.txt.bz2: $(CORPUS_DIR)/corpus-%.txt.bz2
	$(eval lang=$*)
	lbzip2 -d < $(CORPUS_DIR)/corpus-$(lang).txt.bz2 | $(TOOLS_DIR)/corpus-splitter.pl 2000 500 $(lang)-learn.tmp.bz2 $(lang)-test.tmp.bz2
	mv -vf $(lang)-learn.tmp.bz2 $(lang)-learn.txt.bz2
	mv -vf $(lang)-test.tmp.bz2 $(lang)-test.txt.bz2

%-test.txt.bz2: %-learn.txt.bz2

%-full.dict:
	$(eval lang=$*)
	aspell -l $(lang) dump master > $@

grams-%-full.csv.bz2: %-full.dict %-learn.txt.bz2
	$(eval lang=$*)
	set -o pipefail ; lbzip2 -d < $(lang)-learn.txt.bz2 | $(TOOLS_DIR)/import_corpus.py $(lang)-full.dict | sort -rn | lbzip2 -9 > grams-$(lang)-full.csv.bz2.tmp
	mv -f grams-$(lang)-full.csv.bz2.tmp grams-$(lang)-full.csv.bz2

%-predict.dict: grams-%-full.csv.bz2 lang-%.cf
	$(eval lang=$*)
	$(eval words=$(call getconf,lang-$(lang).cf,predict_words))
	$(eval filter=$(call getconf,lang-$(lang).cf,filter_words))
	set -o pipefail ; lbzip2 -d < $< | grep ';#NA;#NA;' | cut -f '1,4' -d';' \
	 | grep -v '#TOTAL' | sort -rn | cut -d';' -f 2 | egrep -v '$(filter)' | tee words-$(lang).txt \
         | sed -n "1,$(words) p" > $(lang)-predict.dict.tmp  # ok i've re-implemented "head" with sed to avoid ugly sigpipes (which hurt with -o pipefail)
	mv -f $(lang)-predict.dict.tmp $@

%.tre: %-full.dict %-predict.dict
	$(eval lang=$*)
	$(TOOLS_DIR)/loadkb.py $(lang)-full.tre < $(lang)-full.dict
	$(TOOLS_DIR)/loadkb.py $@ < $(lang)-predict.dict

grams-%-learn.csv.bz2: %-predict.dict %-learn.txt.bz2
	$(eval lang=$*)
	set -o pipefail	; lbzip2 -d < $(lang)-learn.txt.bz2 | $(TOOLS_DIR)/import_corpus.py $(lang)-predict.dict | lbzip2 -9 > grams-$(lang)-learn.csv.bz2.tmp
	mv -f grams-$(lang)-learn.csv.bz2.tmp grams-$(lang)-learn.csv.bz2

grams-%-test.csv.bz2: %-predict.dict %-test.txt.bz2
	$(eval lang=$*)
	set -o pipefail ; lbzip2 -d < $(lang)-test.txt.bz2 | $(TOOLS_DIR)/import_corpus.py $(lang)-predict.dict | lbzip2 -9 > grams-$(lang)-test.csv.bz2.tmp
	mv -f grams-$(lang)-test.csv.bz2.tmp grams-$(lang)-test.csv.bz2

clusters-%.txt: grams-%-learn.csv.bz2
	$(eval lang=$*)
	@echo "Computing clusters for language $(lang). Please make some coffee ..."
	@echo " (logs can be found in clusters-$(lang).log)"
	set -o pipefail ; lbzip2 -d < $< | sort -rn | sed -n "1,13500000 p" \
	 | $(TOOLS_DIR)/cluster -n 10 -o clusters-$(lang).tmp > clusters-$(lang).log 2>&1
	mv -f clusters-$(lang).tmp $@

predict-%.db: clusters-%.txt grams-%-learn.csv.bz2 lang-%.cf
	$(eval lang=$*)
	$(eval depth=$(call getconf,lang-$(lang).cf,cluster_depth))
	$(eval cgrams=$(call getconf,lang-$(lang).cf,cluster_cgrams))
	$(eval wgrams=$(call getconf,lang-$(lang).cf,cluster_wgrams))
	set -o pipefail ; lbzip2 -d < grams-$(lang)-learn.csv.bz2 \
	 | $(TOOLS_DIR)/clusterize.py -l $(depth) -w $(wgrams) -c $(cgrams) clusters-$(lang).txt \
	 | $(TOOLS_DIR)/load_sqlite.py predict-$(lang)-tmp.db
	mv -f predict-$(lang)-tmp.db $@


