TEMPLATE = aux
OBJECTS_DIR = ./
DESTDIR = ./
CONFIG -= debug_and_release
TARGET = dummy_target
QMAKE_EXTRA_TARGETS += ancillary_make dummy_target
ancillary_make.commands = @make -f makefile all
dummy_target.depends = ancillary_make
