QT       += core

QT       -= gui

TARGET = daemon
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES += sched/main.cpp \
    data/task.cpp \
    data/taskgroup.cpp \
    data/paramset.cpp \
    sched/scheduler.cpp \
    data/crontrigger.cpp \
    data/host.cpp \
    data/hostgroup.cpp \
    sched/taskrequest.cpp \
    sched/executor.cpp

HEADERS += \
    data/task.h \
    data/taskgroup.h \
    data/paramset.h \
    sched/scheduler.h \
    data/crontrigger.h \
    data/host.h \
    data/hostgroup.h \
    sched/taskrequest.h \
    sched/executor.h
