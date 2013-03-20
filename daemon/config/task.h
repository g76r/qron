/* Copyright 2012-2013 Hallowyn and others.
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
#ifndef TASK_H
#define TASK_H

#include <QSharedData>
#include "util/paramset.h"
#include <QSet>
#include "taskgroup.h"

class TaskData;
class QDebug;
class PfNode;
class CronTrigger;
class Scheduler;
class Event;

class Task {
  QSharedDataPointer<TaskData> d;
public:
  Task();
  Task(const Task &other);
  Task(PfNode node, Scheduler *scheduler);
  ~Task();
  Task &operator =(const Task &other);
  bool operator==(const Task &other);
  ParamSet params() const;
  bool isNull() const;
  QSet<QString> noticeTriggers() const;
  /** Fully qualified task name (i.e. "taskGroupId.taskId") */
  QString id() const;
  QString fqtn() const;
  QString label() const;
  QString mean() const;
  QString command() const;
  QString target() const;
  QString infourl() const;
  TaskGroup taskGroup() const;
  void completeConfiguration(TaskGroup taskGroup);
  QList<CronTrigger> cronTriggers() const;
  /** Resources consumed. */
  QMap<QString,qint64> resources() const;
  QString resourcesAsString() const;
  QString triggersAsString() const;
  QDateTime lastExecution() const;
  void setLastExecution(const QDateTime timestamp) const;
  QDateTime nextScheduledExecution() const;
  void setNextScheduledExecution(const QDateTime timestamp) const;
  /** Maximum allowed simultaneous instances (includes running and queued
    * instances). Default: 1. */
  int maxInstances() const;
  /** Current intances count (includes running and queued instances). */
  int instancesCount() const;
  /** Atomic fetch-and-add of the current instances count. */
  int fetchAndAddInstancesCount(int valueToAdd) const;
  const QList<QRegExp> stderrFilters() const;
  void appendStderrFilter(QRegExp filter);
  void triggerStartEvents(const ParamsProvider *context) const;
  void triggerSuccessEvents(const ParamsProvider *context) const;
  void triggerFailureEvents(const ParamsProvider *context) const;
  const QList<Event> onstartEvents() const;
  const QList<Event> onsuccessEvents() const;
  const QList<Event> onfailureEvents() const;
  bool enabled() const;
  void setEnabled(bool enabled) const;
  bool lastSuccessful() const;
  void setLastSuccessful(bool successful) const;
  long long maxExpectedDuration() const;
  long long minExpectedDuration() const;
  ParamSet setenv() const;
  QSet<QString> unsetenv() const;
};

QDebug operator<<(QDebug dbg, const Task &task);

#endif // TASK_H
