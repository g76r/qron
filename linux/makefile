SHELL=bash

.PHONY: default all targz

default: all

all: targz

targz:
	rm -f qrond-linux-amd64.tar.bz2
	strip qrond/{bin,lib}/*
	tar jchf qrond-linux-amd64.tar.bz2 qrond
