TARGETS=taskrequests_statuses.png paramset_hierarchy.png paramset_hierarchy_caption.png alerts_statuses.svg

.PHONY: default all clean re

.SUFFIXES: .dot .png .svg

.dot.png:
	dot -Tpng -o $@ $<

.dot.svg:
	dot -Tsvg -o $@ $<

default: all

all: $(TARGETS)

clean:
	-rm $(TARGETS)

re: clean all

