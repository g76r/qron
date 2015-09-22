/* Copyright 2013-2015 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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
#ifndef STEPACTION_H
#define STEPACTION_H

#include "action.h"
#include "util/paramset.h"

class StepActionData;
class Scheduler;

/** Action activating a given workflow step (more or less the "next" step).
 * Can only be used when called from a workflow subtask event subscription (e.g.
 * "(onsuccess (step foo))" on a workflow subtask).
 * Won't work if triggered in another context. */
class LIBQRONSHARED_EXPORT StepAction : public Action {
public:
  explicit StepAction(Scheduler *scheduler = 0, PfNode node = PfNode());
  StepAction(Scheduler *scheduler, QString stepLocalId);
  StepAction(const StepAction &);
  StepAction &operator=(const StepAction &);
};

#endif // STEPACTION_H
