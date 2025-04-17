/* Copyright 2023-2025 Gregoire Barbier and others.
 * This file is part of libpumpkin, see <http://libpumpkin.g76r.eu/>.
 * Libpumpkin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * Libpumpkin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with libpumpkin.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef QROND_STABLE_H
#define QROND_STABLE_H

// C
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#if defined __cplusplus

// C++
#include <functional>

// Qt
#include <QSortFilterProxyModel>
#include <QCoreApplication>
#include <QFile>
#include <QtDebug>
#include <QThread>
#include <QJsonDocument>

// libp6core
#include "httpd/httpserver.h"
#include "httpd/templatinghttphandler.h"
#include "httpd/uploadhttphandler.h"
#include "httpd/graphvizimagehttphandler.h"
#include "httpd/pipelinehttphandler.h"
#include "textview/htmlitemdelegate.h"
#include "textview/htmltableview.h"
#include "textview/csvtableview.h"
#include "format/stringutils.h"
#include "format/timeformats.h"
#include "format/jsonformats.h"
#include "modelview/paramsetmodel.h"
#include "modelview/shareduiitemslogmodel.h"
#include "log/logrecorditemmodel.h"
#include "log/filelogger.h"
#include "auth/inmemoryrulesauthorizer.h"
#include "auth/inmemoryrulesauthorizer.h"
#include "auth/usersdatabase.h"
#include "io/readonlyresourcescache.h"
#include "io/unixsignalmanager.h"
#include "io/ioutils.h"
#include "thread/atomicvalue.h"
#include "util/radixtree.h"

// libqron
#include "sched/scheduler.h"
#include "configmgt/localconfigrepository.h"
#include "config/requestformfield.h"
#include "config/schedulerconfig.h"
#include "alert/alerter.h"
#include "ui/hostsresourcesavailabilitymodel.h"
#include "ui/resourcesconsumptionmodel.h"
#include "ui/clustersmodel.h"
#include "ui/lastoccuredtexteventsmodel.h"
#include "ui/taskinstancesmodel.h"
#include "ui/tasksmodel.h"
#include "ui/schedulereventsmodel.h"
#include "ui/taskgroupsmodel.h"
#include "ui/configsmodel.h"
#include "ui/confighistorymodel.h"
#include "ui/diagramsbuilder.h"

#endif // __cplusplus

#endif // QROND_STABLE_H
