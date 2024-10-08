# Copyright 2012-2023 Hallowyn and others.
# This file is part of qron, see <http://qron.eu/>.
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

QT       += network sql
QT       -= gui

TARGET = qrond
CONFIG += cmdline largefile c++17 c++20 precompile_header force_debug_info
CONFIG -= app_bundle
TEMPLATE = app

TARGET_OS=default
unix: TARGET_OS=unix
linux: TARGET_OS=linux
android: TARGET_OS=android
macx: TARGET_OS=macx
win32: TARGET_OS=win32
BUILD_TYPE=unknown
CONFIG(debug,debug|release): BUILD_TYPE=debug
CONFIG(release,debug|release): BUILD_TYPE=release

contains(QT_VERSION, ^4\\..*) {
  message("Cannot build with Qt version $${QT_VERSION}.")
  error("Use Qt 5.")
}

exists(/usr/bin/ccache):QMAKE_CXX = \
  CCACHE_SLOPPINESS=pch_defines,time_macros ccache $$QMAKE_CXX
exists(/usr/bin/ccache):QMAKE_CXXFLAGS += -fdiagnostics-color=always
QMAKE_CXXFLAGS += -Wextra -Woverloaded-virtual \
  -Wdouble-promotion -Wimplicit-fallthrough=5 -Wtrampolines \
  -Wduplicated-branches -Wduplicated-cond -Wlogical-op \
  -Wno-padded -Wno-deprecated-copy -Wsuggest-attribute=noreturn \
  -Wsuggest-override -DQT_NO_JAVA_STYLE_ITERATORS -DQT_NO_FOREACH
# LATER add -Wfloat-equal again when QVariant::value<double>() won't trigger it
QMAKE_CXXFLAGS_DEBUG += -ggdb
QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO += -ggdb
!isEmpty(OPTIMIZE_LEVEL):QMAKE_CXXFLAGS_DEBUG += -O$$OPTIMIZE_LEVEL
!isEmpty(OPTIMIZE_LEVEL):QMAKE_CXXFLAGS_RELEASE += -O$$OPTIMIZE_LEVEL
!isEmpty(OPTIMIZE_LEVEL):QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO += -O$$OPTIMIZE_LEVEL

OBJECTS_DIR = ../build-$$TARGET-$$TARGET_OS/$$BUILD_TYPE/obj
RCC_DIR = ../build-$$TARGET-$$TARGET_OS/$$BUILD_TYPE/rcc
MOC_DIR = ../build-$$TARGET-$$TARGET_OS/$$BUILD_TYPE/moc
DESTDIR = ../build-$$TARGET-$$TARGET_OS/$$BUILD_TYPE

# dependency libs
INCLUDEPATH += ../libp6core ../libqron
LIBS += \
  -L../build-qron-$$TARGET_OS/$$BUILD_TYPE \
  -L../build-p6core-$$TARGET_OS/$$BUILD_TYPE
LIBS += -lp6core -lqron

unix {
  ancillary_make.commands = cd $$PWD && make -f ancillary.mf all
  rcc.depends = ancillary_make
  QMAKE_EXTRA_TARGETS += ancillary_make
}

PRECOMPILED_HEADER *= \
    qrond_stable.h

SOURCES *= \
    sched/qrond.cpp \
    wui/webconsole.cpp \
    wui/configuploadhandler.cpp \
    wui/htmlalertitemdelegate.cpp \
    wui/htmllogentryitemdelegate.cpp \
    wui/htmlschedulerconfigitemdelegate.cpp \
    wui/htmltaskinstanceitemdelegate.cpp \
    wui/htmltaskitemdelegate.cpp

HEADERS *= \
    qrond_stable.h \
    sched/qrond.h \
    wui/webconsole.h \
    wui/configuploadhandler.h \
    wui/htmlalertitemdelegate.h \
    wui/htmllogentryitemdelegate.h \
    wui/htmlschedulerconfigitemdelegate.h \
    wui/htmltaskinstanceitemdelegate.h \
    wui/htmltaskitemdelegate.h

RESOURCES *= \
    wui/webconsole.qrc

DISTFILES +=
