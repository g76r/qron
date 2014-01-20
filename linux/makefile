.PHONY: default all targz

default: all

all: targz

targz:
	rm -f qrond-linux-i386.tar.gz
	strip qrond/{bin,lib}/*
	tar zchf qrond-linux-i386.tar.gz qrond
