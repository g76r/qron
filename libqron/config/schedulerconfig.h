/* Copyright 2014 Hallowyn and others.
 * This file is part of qron, see <http://qron.hallowyn.com/>.
 * Qron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * Qron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with qron. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef SCHEDULERCONFIG_H
#define SCHEDULERCONFIG_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include <QHash>
#include <QString>
#include "pf/pfnode.h"
#include "task.h"
#include "cluster.h"
#include "calendar.h"
#include "alerterconfig.h"
#include "accesscontrolconfig.h"
#include "logfile.h"

class SchedulerConfigData;

/** Whole scheduler configuration */
class LIBQRONSHARED_EXPORT SchedulerConfig {
  QSharedDataPointer<SchedulerConfigData> d;
public:
  SchedulerConfig();
  SchedulerConfig(const SchedulerConfig &other);
  explicit SchedulerConfig(PfNode root, Scheduler *scheduler,
                           bool applyLogConfig);
  ~SchedulerConfig();
  SchedulerConfig &operator=(const SchedulerConfig &other);
  bool isNull() const;
  ParamSet globalParams() const;
  ParamSet setenv() const;
  ParamSet unsetenv() const;
  QHash<QString,TaskGroup> tasksGroups() const;
  QHash<QString,Task> tasks() const;
  QHash<QString,Cluster> clusters() const;
  QHash<QString,Host> hosts() const;
  QHash<QString,QHash<QString,qint64> > hostResources() const;
  QHash<QString,Calendar> namedCalendars() const;
  QList<EventSubscription> onstart() const;
  QList<EventSubscription> onsuccess() const;
  QList<EventSubscription> onfailure() const;
  QList<EventSubscription> onlog() const;
  QList<EventSubscription> onnotice() const;
  QList<EventSubscription> onschedulerstart() const;
  QList<EventSubscription> onconfigload() const;
  QList<EventSubscription> allEventsSubscriptions() const;
  qint32 maxtotaltaskinstances() const;
  qint32 maxqueuedrequests() const;
  AlerterConfig alerterConfig() const;
  AccessControlConfig accessControlConfig() const;
  QList<LogFile> logfiles() const;
  QList<Logger*> loggers() const;
  inline bool operator==(const SchedulerConfig &other) const {
    return hash() == other.hash(); }
  inline bool operator<(const SchedulerConfig &other) const {
    return hash() < other.hash(); }
  QString hash() const;
  /** @return number of bytes written or -1 if an error occured */
  qint64 writeAsPf(QIODevice *device) const;
  PfNode toPfNode() const;
  void copyLiveAttributesFromOldTasks(QHash<QString,Task> oldTasks);
  /** Rename cluster, do not perform any sanity check before or after. */
  Cluster renameCluster(QString oldName, QString newName);
  /** Update task, do not perform any sanity check before or after. */
  Task updateTask(Task task);
};

inline uint qHash(SchedulerConfig config) { return qHash(config.hash()); }

#endif // SCHEDULERCONFIG_H
