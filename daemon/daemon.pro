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

QT       += core network
QT       -= gui

TARGET = qrond
CONFIG += console largefile
CONFIG -= app_bundle

INCLUDEPATH += ../libqtpf ../libqtssu
LIBS += -lqtpf -lqtssu
win32:debug:LIBS += -L../../libqtpf/pf-build-windows/debug \
  -L../../libqtpf/pfsql-build-windows/debug \
  -L../../ssu-build-windows/debug
win32:release:LIBS += -L../libqtpf/pf-build-windows/release \
  -L../../libqtpf/pfsql-build-windows/release \
  -L../../ssu-build-windows/release
linux-g++:LIBS += -L../libqtpf/pf -L../libqtpf/pfsql -L../libqtssu

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
    sched/taskrequest.cpp \
    sched/executor.cpp \
    util/timerwithargument.cpp \
    log/log.cpp \
    log/filelogger.cpp \
    ui/taskstreemodel.cpp \
    ui/webconsole.cpp \
    ui/treemodelwithstructure.cpp \
    ui/targetstreemodel.cpp \
    data/cluster.cpp

HEADERS += \
    data/task.h \
    data/taskgroup.h \
    data/paramset.h \
    sched/scheduler.h \
    data/crontrigger.h \
    data/host.h \
    sched/taskrequest.h \
    sched/executor.h \
    util/timerwithargument.h \
    log/log.h \
    log/filelogger.h \
    ui/taskstreemodel.h \
    ui/webconsole.h \
    ui/treemodelwithstructure.h \
    ui/targetstreemodel.h \
    data/cluster.h

RESOURCES += \
    ui/webconsole.qrc
