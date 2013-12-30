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
#ifndef REQUESTTASKACTION_H
#define REQUESTTASKACTION_H

#include "action.h"
#include "util/paramset.h"

class RequestTaskActionData;
class Scheduler;

/** Action requesting for a task execution. */
class RequestTaskAction : public Action {
public:
  RequestTaskAction(Scheduler *scheduler = 0, QString idOrFqtn = QString(),
                   ParamSet params = ParamSet(), bool force = false);
  RequestTaskAction(const RequestTaskAction &);
  ~RequestTaskAction();
};

#endif // REQUESTTASKACTION_H
