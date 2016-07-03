TARGETS=wui/docroot/version.txt

.PHONY: default all clean re

default: all

all: $(TARGETS)

clean:
	-rm $(TARGETS)

re: clean all

wui/docroot/version.txt: FORCE
	OLD="$$(cat $@ 2> /dev/null)"; NEW="$$(git describe --always)"; if [ "$$OLD" != "$$NEW" ]; then echo $$NEW > $@; fi

FORCE:
	
