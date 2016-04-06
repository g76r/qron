TARGETS=user-manual/index.html
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

user-manual/index.html: user-manual.html header.html common-scripts.html footer.html
	cat $< | awk '/<?include:/{sub(".*<\\?include:","");sub("\\?>.*","");system("cat " $$0);next}{print $$0}' > $@
	[ ! -e $(PUBLISHING_DIRECTORY) ] || mkdir $(PUBLISHING_DIRECTORY)/$${CI_BUILD_REF_NAME:-default} && rsync -avrLk --delete --delete-after $$(dirname $@)/. $(PUBLISHING_DIRECTORY)/$${CI_BUILD_REF_NAME:-default}/.
