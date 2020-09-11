SHELL=bash

.PHONY: default all targz docker

default: all

all: targz docker

targz:
	rm -f qrond-linux-amd64.tar.bz2
	strip qrond/{bin,lib}/*
	tar jchf qrond-linux-amd64.tar.bz2 qrond

docker:
	tar czh --exclude 'libQt*' .|docker build -t qrond -
