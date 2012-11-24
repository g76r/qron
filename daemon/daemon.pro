QT       += core

QT       -= gui

TARGET = qrond
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
    sched/executor.cpp \
    pf/pfutils.cpp \
    pf/pfparser.cpp \
    pf/pfoptions.cpp \
    pf/pfnode.cpp \
    pf/pfhandler.cpp \
    pf/pfdomhandler.cpp \
    pf/pfcontent.cpp \
    pf/pfarray.cpp \
    util/ioutils.cpp \
    util/timerwithargument.cpp \
    log/log.cpp \
    log/filelogger.cpp

HEADERS += \
    data/task.h \
    data/taskgroup.h \
    data/paramset.h \
    sched/scheduler.h \
    data/crontrigger.h \
    data/host.h \
    data/hostgroup.h \
    sched/taskrequest.h \
    sched/executor.h \
    pf/pfutils.h \
    pf/pfparser.h \
    pf/pfoptions.h \
    pf/pfnode.h \
    pf/pfinternals.h \
    pf/pfhandler.h \
    pf/pfdomhandler.h \
    pf/pfcontent.h \
    pf/pfarray.h \
    util/ioutils.h \
    util/timerwithargument.h \
    log/log.h \
    log/filelogger.h
