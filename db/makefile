TOOLS=../tools

all: fr-full.tre en-full.tre

fr-full.tre:
	aspell -l fr dump master | $(TOOLS)/loadkb.py fr-full.tre

en-full.tre:
	aspell -l en dump master | $(TOOLS)/loadkb.py en-full.tre
