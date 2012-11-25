QT       += core
QT       -= gui

TARGET = qrond
CONFIG   += console
CONFIG   -= app_bundle

INCLUDEPATH += ../libqtpf
LIBS += -lqtpf -L../libqtpf

QMAKE_CXXFLAGS += -Wextra
linux-g++ {
  OBJECTS_DIR = ../daemon-build-linux/obj
  RCC_DIR = ../daemon-build-linux/rcc
  MOC_DIR = ../daemon-build-linux/moc
}

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
    util/timerwithargument.h \
    log/log.h \
    log/filelogger.h
