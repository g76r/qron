SHELL=bash
PUBLISHING_DIRECTORY=/publishing/qron/pkg
GIT_DESCRIBE=$(shell git describe --tags --always 2> /dev/null || echo master)
GIT_NAME=$${CI_COMMIT_REF_NAME:-$(GIT_DESCRIBE)}

.PHONY: default all targz docker FORCE

default: all

all: targz docker

targz: FORCE
	rm -f qrond-linux-amd64.tar.bz2
	strip qrond/{bin,lib}/*
	tar jchf qrond-linux-amd64.tar.bz2 qrond
	if [ -e $(PUBLISHING_DIRECTORY) ]; then cp qrond-linux-amd64.tar.bz2 $(PUBLISHING_DIRECTORY)/qrond-linux-amd64-$${CI_COMMIT_REF_NAME:-master}.tar.bz2; fi

docker: FORCE
	echo $(GIT_NAME) / $(GIT_DESCRIBE)
	if which docker > /dev/null 2>&1; then docker build -t qrond . && docker tag qrond g76r/qrond:$${CI_COMMIT_SHA:-master} && docker tag qrond g76r/qrond:$(GIT_NAME); fi
	-if which docker > /dev/null 2>&1; then docker push g76r/qrond:$${CI_COMMIT_SHA:-master}; docker push g76r/qrond:$(GIT_NAME); fi

FORCE:
