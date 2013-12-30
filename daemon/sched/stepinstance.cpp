/* Copyright 2013 Hallowyn and others.
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

class StepInstanceData : public QSharedData {
public:
  Step _step;
  bool _ready;
  QSet<QString> _pendingPredecessors;
  TaskRequest _taskInstance;
  StepInstanceData(Step step = Step(), TaskRequest taskInstance = TaskRequest())
    : _step(step), _ready(false), _pendingPredecessors(step.predecessors()),
      _taskInstance(taskInstance) { }
};

StepInstance::StepInstance() {
}

StepInstance::StepInstance(Step step, TaskRequest taskInstance)
  : d(new StepInstanceData(step, taskInstance)) {
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
                                    const ParamsProvider *context) {
  if (!d)
    return;
  d->_pendingPredecessors.remove(predecessor);
  switch (d->_step.kind()) {
  case Step::AndJoin:
    if (!d->_ready && d->_pendingPredecessors.isEmpty()) {
      d->_ready = true;
      d->_step.triggerReadyEvents(context);
    }
    break;
  case Step::OrJoin:
    if (!d->_ready) {
      d->_ready = true;
      d->_step.triggerReadyEvents(context);
    }
    break;
  case Step::SubTask:
  case Step::Unknown:
    // nothing to do
    break;
  }
}

TaskRequest StepInstance::taskInstance() const {
  return d ? d->_taskInstance : TaskRequest();
}
