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
#include "stepinstance.h"
#include <QSharedData>
#include "executor.h"

class StepInstanceData : public QSharedData {
public:
  Step _step;
  mutable bool _ready;
  // note that QSet is not thread safe, however it is only modified by
  // predecessorReady() which is only called by one thread (the executor thread
  // handling the workflow)
  mutable QSet<QString> _pendingPredecessors;
  TaskInstance _workflowTaskInstance;
  QPointer<Executor> _executor;
  StepInstanceData(Step step = Step(),
                   TaskInstance workflowTaskInstance = TaskInstance(),
                   QPointer<Executor> executor = QPointer<Executor>())
    : _step(step), _ready(false), _pendingPredecessors(step.predecessors()),
      _workflowTaskInstance(workflowTaskInstance), _executor(executor) { }
};

StepInstance::StepInstance() {
}

StepInstance::StepInstance(Step step, TaskInstance workflowTaskInstance,
                           QPointer<Executor> executor)
  : d(new StepInstanceData(step, workflowTaskInstance, executor)) {
  // TODO remove useless reference to executor
}

StepInstance::~StepInstance() {
}

StepInstance::StepInstance(const StepInstance &rhs) : d(rhs.d) {
}

StepInstance &StepInstance::operator=(const StepInstance &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

Step StepInstance::step() const {
  return d ? d->_step : Step();
}

bool StepInstance::isReady() const {
  return d ? d->_ready : false;
}

void StepInstance::predecessorReady(QString predecessor,
                                    ParamSet eventContext) const {
  if (!d)
    return;
  d->_pendingPredecessors.remove(predecessor);
  switch (d->_step.kind()) {
  case Step::AndJoin:
    if (!d->_ready && d->_pendingPredecessors.isEmpty()) {
      d->_ready = true;
      d->_step.triggerReadyEvents(d->_workflowTaskInstance, eventContext);
    }
    break;
  case Step::OrJoin:
  case Step::SubTask: // a subtask step is actually an implicit or join
    if (!d->_ready) {
      d->_ready = true;
      d->_step.triggerReadyEvents(d->_workflowTaskInstance, eventContext);
    }
    break;
  case Step::Unknown:
    Log::error(d->_workflowTaskInstance.task().id(),
               d->_workflowTaskInstance.id())
        << "StepInstance::predecessorReady called on unknown step kind";
  }
}

TaskInstance StepInstance::workflowTaskInstance() const {
  return d ? d->_workflowTaskInstance : TaskInstance();
}
