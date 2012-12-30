/* Copyright 2012 Hallowyn and others.
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
#include "paramset.h"
#include <QSet>

class TaskData;
class QDebug;
class PfNode;
class TaskGroup;
class CronTrigger;

class Task {
  QSharedDataPointer<TaskData> d;
public:
  Task();
  Task(const Task &other);
  Task(PfNode node);
  ~Task();
  Task &operator =(const Task &other);
  ParamSet params() const;
  bool isNull() const;
  QSet<QString> eventTriggers() const;
  /** Fully qualified task name (i.e. "taskGroupId.taskId")
    */
  QString id() const;
  QString fqtn() const;
  QString label() const;
  QString mean() const;
  QString command() const;
  QString target() const;
  void setTaskGroup(TaskGroup taskGroup);
  QList<CronTrigger> cronTriggers() const;
  /** Resources consumed. */
  QMap<QString,qint64> resources() const;
  QString resourcesAsString() const;
  QString triggersAsString() const;
  QDateTime lastExecution() const;
  void setLastExecution(const QDateTime timestamp) const;
  QDateTime nextScheduledExecution() const;
  void setNextScheduledExecution(const QDateTime timestamp) const;
};

QDebug operator<<(QDebug dbg, const Task &task);

#endif // TASK_H
