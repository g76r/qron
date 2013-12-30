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
#ifndef STEPINSTANCE_H
#define STEPINSTANCE_H

#include <QSharedDataPointer>
#include "config/step.h"
#include "taskinstance.h"

class StepInstanceData;

class StepInstance {
  QSharedDataPointer<StepInstanceData> d;

public:
  StepInstance();
  StepInstance(Step step, TaskInstance workflowTaskInstance);
  StepInstance(const StepInstance &);
  StepInstance &operator=(const StepInstance &);
  ~StepInstance();
  bool isNull() const;
  Step step() const;
  bool isReady() const;
  /** Method to be called by Executor every time a predecessor is ready */
  void predecessorReady(QString predecessor);
  TaskInstance workflowTaskInstance() const;
  TaskInstance subtaskInstance() const;
  void setSubtaskInstance(TaskInstance subtask);
};

#endif // STEPINSTANCE_H
