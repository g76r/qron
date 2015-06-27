TEMPLATE = aux
OBJECTS_DIR = ./
DESTDIR = ./
CONFIG -= debug_and_release
first.commands = @make -f makefile
QMAKE_EXTRA_TARGETS += first
