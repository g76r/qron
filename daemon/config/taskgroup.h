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
#ifndef TASKGROUP_H
#define TASKGROUP_H

#include <QSharedData>
#include <QList>
#include "util/paramset.h"

class TaskGroupData;
class Task;
class PfNode;
class QDebug;

class TaskGroup {
  QSharedDataPointer<TaskGroupData> d;

public:
  TaskGroup();
  TaskGroup(const TaskGroup &other);
  TaskGroup(PfNode node, ParamSet parentParamSet);
  ~TaskGroup();
  TaskGroup &operator =(const TaskGroup &other);
  //QList<Task> tasks();
  QString id() const;
  QString label() const;
  ParamSet params() const;
  bool isNull() const;
};

QDebug operator<<(QDebug dbg, const TaskGroup &taskGroup);

#endif // TASKGROUP_H