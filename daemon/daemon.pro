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
win32:debug:LIBS += -L../libqtpf/pf-build-windows/debug \
  -L../libqtpf/pfsql-build-windows/debug \
  -L../libqtssu-build-windows/debug
win32:release:LIBS += -L../libqtpf/pf-build-windows/release \
  -L../libqtpf/pfsql-build-windows/release \
  -L../libqtssu-build-windows/release
unix:LIBS += -L../libqtpf/pf -L../libqtpf/pfsql -L../libqtssu
LIBS += -lqtpf -lqtssu

QMAKE_CXXFLAGS += -Wextra
#QMAKE_CXXFLAGS += -fno-elide-constructors
unix:debug:QMAKE_CXXFLAGS += -ggdb

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

SOURCES += \
    config/task.cpp \
    config/taskgroup.cpp \
    sched/scheduler.cpp \
    config/crontrigger.cpp \
    config/host.cpp \
    sched/taskrequest.cpp \
    sched/executor.cpp \
    ui/webconsole.cpp \
    config/cluster.cpp \
    ui/hostslistmodel.cpp \
    ui/clusterslistmodel.cpp \
    ui/resourcesallocationmodel.cpp \
    alert/alerter.cpp \
    alert/alert.cpp \
    config/alertrule.cpp \
    alert/alertchannel.cpp \
    alert/udpalertchannel.cpp \
    alert/mailalertchannel.cpp \
    alert/logalertchannel.cpp \
    alert/httpalertchannel.cpp \
    alert/execalertchannel.cpp \
    ui/raisedalertsmodel.cpp \
    ui/alertrulesmodel.cpp \
    ui/taskrequestsmodel.cpp \
    ui/tasksmodel.cpp \
    event/event.cpp \
    event/postnoticeevent.cpp \
    event/logevent.cpp \
    event/udpevent.cpp \
    event/httpevent.cpp \
    event/raisealertevent.cpp \
    event/cancelalertevent.cpp \
    event/emitalertevent.cpp \
    event/requesttaskevent.cpp \
    ui/schedulereventsmodel.cpp \
    ui/lastoccuredtexteventsmodel.cpp \
    ui/taskgroupsmodel.cpp \
    sched/qrond.cpp \
    config/configutils.cpp \
    ui/alertchannelsmodel.cpp \
    config/requestformfield.cpp \
    ui/resourcesconsumptionmodel.cpp \
    config/logfile.cpp \
    ui/logfilesmodel.cpp \
    config/calendar.cpp \
    ui/calendarsmodel.cpp \
    ui/htmltaskitemdelegate.cpp \
    ui/htmltaskrequestitemdelegate.cpp \
    ui/htmlalertitemdelegate.cpp

HEADERS += \
    config/task.h \
    config/taskgroup.h \
    sched/scheduler.h \
    config/crontrigger.h \
    config/host.h \
    sched/taskrequest.h \
    sched/executor.h \
    ui/webconsole.h \
    config/cluster.h \
    ui/hostslistmodel.h \
    ui/clusterslistmodel.h \
    ui/resourcesallocationmodel.h \
    alert/alerter.h \
    alert/alert.h \
    config/alertrule.h \
    alert/alertchannel.h \
    alert/udpalertchannel.h \
    alert/mailalertchannel.h \
    alert/logalertchannel.h \
    alert/httpalertchannel.h \
    alert/execalertchannel.h \
    ui/raisedalertsmodel.h \
    ui/alertrulesmodel.h \
    ui/taskrequestsmodel.h \
    ui/tasksmodel.h \
    event/event.h \
    event/postnoticeevent.h \
    event/event_p.h \
    event/logevent.h \
    event/udpevent.h \
    event/httpevent.h \
    event/raisealertevent.h \
    event/cancelalertevent.h \
    event/emitalertevent.h \
    event/requesttaskevent.h \
    ui/schedulereventsmodel.h \
    ui/lastoccuredtexteventsmodel.h \
    ui/taskgroupsmodel.h \
    sched/qrond.h \
    config/configutils.h \
    ui/alertchannelsmodel.h \
    config/requestformfield.h \
    ui/resourcesconsumptionmodel.h \
    config/logfile.h \
    ui/logfilesmodel.h \
    config/calendar.h \
    ui/calendarsmodel.h \
    ui/htmltaskitemdelegate.h \
    ui/htmltaskrequestitemdelegate.h \
    ui/htmlalertitemdelegate.h

RESOURCES += \
    ui/webconsole.qrc
