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
#include "step.h"
#include <QSharedData>
#include "pf/pfnode.h"
#include "task.h"
#include "sched/scheduler.h"
#include "log/log.h"
#include "configutils.h"
#include "action/requesttaskaction.h"

class StepData : public QSharedData {
public:
  QString _id, _fqsn, _workflowId;
  Step::Kind _kind;
  Task _subtask;
  QPointer<Scheduler> _scheduler;
  QSet<QString> _predecessors;
  QList<EventSubscription> _onready;
  StepData() : _kind(Step::Unknown) { }
};

Step::Step() {
}

Step::Step(PfNode node, Scheduler *scheduler, TaskGroup taskGroup,
           QString workflowTaskId, QHash<QString, Calendar> namedCalendars) {
  StepData *sd = new StepData;
  sd->_scheduler = scheduler;
  sd->_id = ConfigUtils::sanitizeId(node.contentAsString(), false);
  sd->_fqsn = taskGroup.id()+"."+workflowTaskId+":"+sd->_id;
  sd->_workflowId = taskGroup.id()+"."+workflowTaskId;
  if (node.name() == "and") {
    sd->_kind = Step::AndJoin;
    ConfigUtils::loadEventSubscription(node, "onready", sd->_fqsn,
                                       &sd->_onready, scheduler);
    if (node.hasChild("onsuccess") || node.hasChild("onfailure")
        || node.hasChild("onfinish") || node.hasChild("onstart"))
      Log::warning() << "ignoring other events than onready in non-subtask "
                        "step: " << node.toString();
  } else if (node.name() == "or") {
    sd->_kind = Step::OrJoin;
    ConfigUtils::loadEventSubscription(node, "onready", sd->_fqsn,
                                       &sd->_onready, scheduler);
    if (node.hasChild("onsuccess") || node.hasChild("onfailure")
        || node.hasChild("onfinish") || node.hasChild("onstart"))
      Log::warning() << "ignoring other events than onready in non-subtask "
                        "step: " << node.toString();
  } else if (node.name() == "subtask") {
    sd->_kind = Step::SubTask;
    if (node.hasChild("taskgroup"))
      Log::warning() << "ignoring subtask taskgroup: " << node.toString();
    if (node.hasChild("trigger"))
      Log::warning() << "ignoring subtask triggers: " << node.toString();
    if (node.hasChild("onready"))
      Log::warning() << "ignoring subtask onready event: " << node.toString();
    node.setContent(workflowTaskId+"-"+node.contentAsString());
    sd->_subtask = Task(node, scheduler, taskGroup, sd->_workflowId,
                        namedCalendars);
    if (sd->_subtask.isNull()) {
      Log::error() << "step with invalid subtask: " << node.toString();
      delete sd;
      return;
    }
    sd->_onready.append(
          EventSubscription(sd->_fqsn, "onready",
                            RequestTaskAction(scheduler, sd->_subtask.id())));
  } else {
      Log::error() << "unsupported step kind: " << node.toString();
      delete sd;
      return;
  }
  d = sd;
}

Step::~Step() {
}

Step::Step(const Step &rhs) : d(rhs.d) {
}

Step &Step::operator=(const Step &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

bool Step::operator==(const Step &other) const {
  return (isNull() && other.isNull()) || fqsn() == other.fqsn();
}

bool Step::operator<(const Step &other) const {
  return fqsn() < other.fqsn();
}

QString Step::id() const {
  return d ? d->_id : QString();
}

QString Step::fqsn() const {
  return d ? d->_fqsn : QString();
}

Step::Kind Step::kind() const {
  return d ? d->_kind : Step::Unknown;
}

Task Step::subtask() const {
  return d ? d->_subtask : Task();
}

QString Step::workflowId() const {
  return d ? d->_workflowId : QString();
}

QSet<QString> Step::predecessors() const {
  return d ? d->_predecessors : QSet<QString>();
}

void Step::insertPredecessor(QString predecessor) {
  if (d)
    d->_predecessors.insert(predecessor);
}

void Step::triggerReadyEvents(TaskInstance workflowTaskInstance,
                              ParamSet eventContext) const {
  if (d) {
    //eventContext.setValue("!stepid", d->_id);
    //eventContext.setValue("!fqsn", d->_fqsn);
    foreach (EventSubscription sub, d->_onready)
      sub.triggerActions(eventContext, workflowTaskInstance);
  }
}

QList<EventSubscription> Step::onreadyEventSubscriptions() const {
  return d ? d->_onready : QList<EventSubscription>();
}

QString Step::kindToString(Kind kind) {
  switch (kind) {
  case SubTask:
    return "subtask";
  case AndJoin:
    return "and";
  case OrJoin:
    return "or";
  case Unknown:
    ;
  }
  return "unknown";
}

PfNode Step::toPfNode() const {
  if (!d)
    return PfNode();
  PfNode node(kindToString(d->_kind), d->_id);
  foreach (const PfNode &child, d->_subtask.toPfNode().children()) {
    const QString &name = child.name();
    if (name != "taskgroup" && name != "trigger")
      node.appendChild(child);
  }
  if (d->_kind != SubTask)
    ConfigUtils::writeEventSubscriptions(&node, d->_onready);
  return node;
}
