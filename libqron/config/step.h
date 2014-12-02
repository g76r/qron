/* Copyright 2013-2014 Hallowyn and others.
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
#ifndef STEP_H
#define STEP_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include "util/paramset.h"

class StepData;
class Task;
class EventSubscription;
class PfNode;
class Scheduler;
class TaskInstance;
class Calendar;
class TaskGroup;

/** Step of a workflow task. */
class LIBQRONSHARED_EXPORT Step {
  QSharedDataPointer<StepData> d;
public:
  enum Kind { Unknown, SubTask, AndJoin, OrJoin };

  Step();
  Step(const Step &);
  Step(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
       QString workflowTaskId, QHash<QString, Calendar> namedCalendars);
  Step &operator=(const Step &);
  ~Step();
  bool operator==(const Step &other) const;
  /** compare fqsn */
  bool operator<(const Step &other) const;
  bool isNull() const { return !d; }
  /** Step id within the workflow, not unique across workflows.
   * @see fqsn() */
  QString id() const;
  /** Fully qualified step name.
   * Equal to workflow task id + ':' + step id, e.g. "group1.task1:step1"
   * Every step has a fqsn whereas only subtask steps have a task id
   * @see workflowId()
   * @see id() */
  QString fqsn() const;
  Kind kind() const;
  static QString kindToString(Kind kind);
  QString kindToString() const {
    return kindToString(kind()); }
  Task subtask() const;
  QString workflowId() const;
  QSet<QString> predecessors() const;
  void insertPredecessor(QString predecessor);
  void triggerReadyEvents(TaskInstance workflowTaskInstance,
                          ParamSet eventContext) const;
  QList<EventSubscription> onreadyEventSubscriptions() const;
  PfNode toPfNode() const;
};

#endif // STEP_H
