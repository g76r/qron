# Copyright 2012 Hallowyn and others.
# This file is part of qron, see <http://qron.hallowyn.com/>.
# Qron is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# Qron is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
# You should have received a copy of the GNU Affero General Public License
# along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

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

contains(QT_VERSION, ^4\\.[0-6]\\..*) {
  message("Cannot build Qt Creator with Qt version $${QT_VERSION}.")
  error("Use at least Qt 4.7.")
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
