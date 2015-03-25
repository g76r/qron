/* Copyright 2013-2015 Hallowyn and others.
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
#include "modelview/shareduiitem.h"

class StepData;
class Task;
class EventSubscription;
class PfNode;
class Scheduler;
class TaskInstance;
class Calendar;
class TaskGroup;
class Trigger;
class WorkflowTransition;

/** Step of a workflow task. */
class LIBQRONSHARED_EXPORT Step : public SharedUiItem {
public:
  enum Kind { Unknown, SubTask, AndJoin, OrJoin, Start, End, WorkflowTrigger };

  Step();
  Step(const Step &other);
  Step(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
       QString workflowTaskId, QHash<QString, Calendar> namedCalendars);
  Step(QString localId, Trigger trigger, EventSubscription es, QString workflowTaskId);
  Step &operator=(const Step &other) {
    SharedUiItem::operator=(other); return *this; }
  /** Fully qualified step name.
   * Equal to workflow task id + ':' + step id, e.g. "group1.task1:step1"
   * Every step has a fqsn whereas only subtask steps have a task id
   * @see workflowId()
   * @see id() */
  QString localId() const;
  Kind kind() const;
  static QString kindAsString(Kind kind);
  QString kindAsString() const { return kindAsString(kind()); }
  Kind kindFromString(QString kind);
  Task subtask() const;
  QString workflowId() const;
  void setWorkflowId(QString workflowId);
  Trigger trigger() const;
  QSet<WorkflowTransition> predecessors() const;
  void insertPredecessor(WorkflowTransition predecessor);
  void triggerReadyEvents(TaskInstance workflowTaskInstance,
                          ParamSet eventContext) const;
  QList<EventSubscription> onreadyEventSubscriptions() const;
  void appendOnReadyStep(Scheduler *scheduler, QString localStepId);
  PfNode toPfNode() const;
  bool setUiData(int section, const QVariant &value, QString *errorString = 0,
                 int role = Qt::EditRole,
                 const SharedUiItemDocumentManager *dm = 0);

private:
  StepData *data();
  const StepData *data() const { return (const StepData*)SharedUiItem::data(); }
};

#endif // STEP_H
