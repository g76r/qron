TARGETS=taskrequests_statuses.png paramset_hierarchy.png

.PHONY: default all clean re

.SUFFIXES: .dot .png

.dot.png:
	dot -Tpng -o $@ $<

default: all

all: $(TARGETS)

clean:
	-rm $(TARGETS)

re: clean all

