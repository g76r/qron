SHELL=bash
PUBLISHING_DIRECTORY=/publishing/qron/pkg
GIT_DESCRIBE=$(shell git describe --tags --always 2> /dev/null || echo latest)
GIT_NAME=$${CI_COMMIT_REF_NAME:-$(GIT_DESCRIBE)}
DOCKER_REGISTRY=g76r/qrond

.PHONY: default all targz docker FORCE
.ONESHELL:

default: all

ifeq ($(DOCKER_BUILD_ENABLED),1)
all: targz docker
else
all: targz
endif

targz: FORCE
	rm -f qrond-linux-amd64.tar.bz2
	strip qrond/{bin,lib}/*
	tar jchf qrond-linux-amd64.tar.bz2 qrond
	if [ -e $(PUBLISHING_DIRECTORY) ]; then
	  cp qrond-linux-amd64.tar.bz2 $(PUBLISHING_DIRECTORY)/qrond-linux-amd64-$${CI_COMMIT_REF_NAME:-latest}.tar.bz2
	fi

docker: FORCE
	echo $(GIT_NAME) / $(GIT_DESCRIBE)
	docker build -t qrond .
	docker tag qrond $(DOCKER_REGISTRY):$${CI_COMMIT_SHA:-latest}
	docker tag qrond $(DOCKER_REGISTRY):$(GIT_NAME)
	docker tag qrond $(DOCKER_REGISTRY):latest
	docker push $(DOCKER_REGISTRY):$${CI_COMMIT_SHA:-latest}
	docker push $(DOCKER_REGISTRY):$(GIT_NAME)
	docker push $(DOCKER_REGISTRY):latest

FORCE:
