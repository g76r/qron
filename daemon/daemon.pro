# Copyright 2012-2014 Hallowyn and others.
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
# along with qron.  If not, see <http://www.gnu.org/licenses/>.

QT       += core network
QT       -= gui

TARGET = qrond
CONFIG += console largefile
CONFIG -= app_bundle

INCLUDEPATH += ../libqtpf ../libqtssu ../libqron
win32:debug:LIBS += -L../libqtpf/build-libqtpf-windows/debug \
  -L../libqtpf/build-libqtpfsql-windows/debug \
  -L../libqtpf/build-libqron-windows/debug \
  -L../build-libqtssu-windows/debug
win32:release:LIBS += -L../libqtpf/build-libqtpf-windows/release \
  -L../libqtpf/build-libqtpfsql-windows/release \
  -L../libqtpf/build-libqron-windows/release \
  -L../build-libqtssu-windows/release
unix:LIBS += -L../libqtpf/libqtpf -L../libqtpf/libqtpfsql -L../libqtssu \
  -L../libqron
LIBS += -lqtpf -lqtssu -lqron


QMAKE_CXXFLAGS += -Wextra
#QMAKE_CXXFLAGS += -fno-elide-constructors
unix:debug:QMAKE_CXXFLAGS += -ggdb

unix {
  OBJECTS_DIR = ../build-daemon-unix/obj
  RCC_DIR = ../build-daemon-unix/rcc
  MOC_DIR = ../build-daemon-unix/moc
}

contains(QT_VERSION, ^4\\..*) {
  message("Cannot build with Qt version $${QT_VERSION}.")
  error("Use Qt 5.")
}

TEMPLATE = app

SOURCES += \
    sched/qrond.cpp \
    wui/webconsole.cpp

HEADERS += \
    sched/qrond.h \
    wui/webconsole.h

RESOURCES += \
    wui/webconsole.qrc

