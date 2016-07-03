TARGETS=wui/docroot/version.txt

.PHONY: default all clean re

default: all

all: $(TARGETS)
	$(MAKE) -f Makefile all

clean:
	$(MAKE) -f Makefile clean
	rm -f $(TARGETS)

re: clean all

wui/docroot/version.txt: FORCE
	OLD="$$(cat $@ 2> /dev/null)"; NEW="$$(git describe --always)"; if [ "$$OLD" != "$$NEW" ]; then echo $$NEW > $@; fi

FORCE:
	
