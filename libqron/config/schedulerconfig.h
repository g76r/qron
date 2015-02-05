/* Copyright 2014-2015 Hallowyn and others.
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
#include "modelview/shareduiitem.h"

class SchedulerConfigData;

/** Whole scheduler configuration */
class LIBQRONSHARED_EXPORT SchedulerConfig : public SharedUiItem {
public:
  SchedulerConfig();
  SchedulerConfig(const SchedulerConfig &other);
  /**
   * @param scheduler if 0, many scheduler-dependant objects (such as Action and
   * it subclasses) won't be able to behave fully correctly but the whole
   * parsing/writing/reading processes will work, which is convenient if the
   * config is not used within a scheduler but rather within a user interface
   * such as a configuration editor
   * @param applyLogConfig if true, call applyLogConfig() during configuration
   * parsing, as soon as possible, which may help having more detailed warning
   * and error messages about the configuration itself
   */
  SchedulerConfig(PfNode root, Scheduler *scheduler, bool applyLogConfig);
  ~SchedulerConfig();
  /** Should only be used by SharedUiItemsModels to get size and headers from
   * a non-null item. */
  static SchedulerConfig templateSchedulerConfig();
  SchedulerConfig &operator=(const SchedulerConfig &other) {
    SharedUiItem::operator=(other); return *this; }
  bool isNull() const;
  ParamSet globalParams() const;
  ParamSet setenv() const;
  ParamSet unsetenv() const;
  QHash<QString,TaskGroup> tasksGroups() const;
  QHash<QString,Task> tasks() const;
  QHash<QString,Cluster> clusters() const;
  QHash<QString,Host> hosts() const;
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
  void changeTask(Task newItem, Task oldItem);
  void changeCluster(Cluster newItem, Cluster oldItem);
  void changeHost(Host newItem, Host oldItem);
  // TODO change methods for other objects
  void applyLogConfig() const;

private:
  SchedulerConfigData *data();
  const SchedulerConfigData *data() const {
    return (const SchedulerConfigData*)SharedUiItem::data(); }
};

inline uint qHash(SchedulerConfig config) { return qHash(config.hash()); }

#endif // SCHEDULERCONFIG_H
