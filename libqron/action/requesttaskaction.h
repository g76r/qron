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
#ifndef REQUESTTASKACTION_H
#define REQUESTTASKACTION_H

#include "action.h"
#include "util/paramset.h"

class RequestTaskActionData;
class Scheduler;

/** Action requesting for a task execution.
 * Id parameter can be either fully qualified ("group.shortid") or short and
 * local to caller task group if any, which is usefull if this action has been
 * triggered by a task onsuccess/onfailure/onfinish event.
 * In addition id parameter is evaluated within event context.
 */
class LIBQRONSHARED_EXPORT RequestTaskAction : public Action {
public:
  explicit RequestTaskAction(Scheduler *scheduler = 0, QString id = QString(),
                             ParamSet params = ParamSet(), bool force = false);
  RequestTaskAction(const RequestTaskAction &);
  ~RequestTaskAction();
};

#endif // REQUESTTASKACTION_H
