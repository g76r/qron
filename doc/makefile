TARGETS=user-manual/user-manual.html
PUBLISHING_DIRECTORY=/publishing/qron/user-manual

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

user-manual/user-manual.html: user-manual.html header.html common-scripts.html footer.html
	cat $< | awk '/<?include:/{sub(".*<\\?include:","");sub("\\?>.*","");system("cat " $$0);next}{print $$0}' > $@
	[ ! -e $(PUBLISHING_DIRECTORY) ] || rsync -avrLk $$(dirname $@)/. $(PUBLISHING_DIRECTORY)/.
