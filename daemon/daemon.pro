# Copyright 2012-2013 Hallowyn and others.
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

INCLUDEPATH += ../libqtpf ../libqtssu
LIBS += -lqtpf -lqtssu
win32:debug:LIBS += -L../../libqtpf/pf-build-windows/debug \
  -L../../libqtpf/pfsql-build-windows/debug \
  -L../../ssu-build-windows/debug
win32:release:LIBS += -L../libqtpf/pf-build-windows/release \
  -L../../libqtpf/pfsql-build-windows/release \
  -L../../ssu-build-windows/release
unix:LIBS += -L../libqtpf/pf -L../libqtpf/pfsql -L../libqtssu

QMAKE_CXXFLAGS += -Wextra
unix {
  OBJECTS_DIR = ../daemon-build-unix/obj
  RCC_DIR = ../daemon-build-unix/rcc
  MOC_DIR = ../daemon-build-unix/moc
}

contains(QT_VERSION, ^4\\.[0-6]\\..*) {
  message("Cannot build Qt Creator with Qt version $${QT_VERSION}.")
  error("Use at least Qt 4.7.")
}

TEMPLATE = app

SOURCES += sched/main.cpp \
    config/task.cpp \
    config/taskgroup.cpp \
    sched/scheduler.cpp \
    config/crontrigger.cpp \
    config/host.cpp \
    config/taskrequest.cpp \
    sched/executor.cpp \
    ui/taskstreemodel.cpp \
    ui/webconsole.cpp \
    ui/treemodelwithstructure.cpp \
    ui/targetstreemodel.cpp \
    config/cluster.cpp \
    ui/textmatrixmodel.cpp \
    ui/hostslistmodel.cpp \
    ui/clusterslistmodel.cpp \
    ui/resourcesallocationmodel.cpp \
    alert/alerter.cpp \
    config/alert.cpp \
    config/alertrule.cpp \
    alert/alertchannel.cpp \
    alert/udpalertchannel.cpp \
    alert/mailalertchannel.cpp \
    alert/logalertchannel.cpp \
    alert/setflagalertchannel.cpp \
    alert/clearflagalertchannel.cpp \
    alert/posteventalertchannel.cpp \
    alert/httpalertchannel.cpp \
    alert/execalertchannel.cpp \
    ui/textsetmodel.cpp \
    ui/raisedalertsmodel.cpp \
    ui/lastemitedalertsmodel.cpp \
    ui/alertrulesmodel.cpp \
    ui/taskrequestsmodel.cpp \
    ui/tasksmodel.cpp

HEADERS += \
    config/task.h \
    config/taskgroup.h \
    sched/scheduler.h \
    config/crontrigger.h \
    config/host.h \
    config/taskrequest.h \
    sched/executor.h \
    ui/taskstreemodel.h \
    ui/webconsole.h \
    ui/treemodelwithstructure.h \
    ui/targetstreemodel.h \
    config/cluster.h \
    ui/textmatrixmodel.h \
    ui/hostslistmodel.h \
    ui/clusterslistmodel.h \
    ui/textviews.h \
    ui/resourcesallocationmodel.h \
    alert/alerter.h \
    config/alert.h \
    config/alertrule.h \
    alert/alertchannel.h \
    alert/udpalertchannel.h \
    alert/mailalertchannel.h \
    alert/logalertchannel.h \
    alert/setflagalertchannel.h \
    alert/clearflagalertchannel.h \
    alert/posteventalertchannel.h \
    alert/httpalertchannel.h \
    alert/execalertchannel.h \
    ui/textsetmodel.h \
    ui/raisedalertsmodel.h \
    ui/lastemitedalertsmodel.h \
    ui/alertrulesmodel.h \
    ui/taskrequestsmodel.h \
    ui/tasksmodel.h

RESOURCES += \
    ui/webconsole.qrc
