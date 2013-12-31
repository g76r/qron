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
#ifndef ACTION_P_H
#define ACTION_P_H

#include "action.h"
#include <QPointer>
#include "sched/scheduler.h"
#include "config/eventsubscription.h"

class ActionData : public QSharedData {
public:
  QPointer<Scheduler> _scheduler;
  explicit ActionData(Scheduler *scheduler = 0) : _scheduler(scheduler) { }
  virtual ~ActionData();
  /** Human readable description of action */
  virtual QString toString() const;
  /** Type of action for programmatic test, e.g. "postnotice" */
  virtual QString actionType() const;
  /** Default: do nothing */
  virtual void trigger(EventSubscription subscription,
                       const ParamsProvider *context) const;
  /** Default: call trigger() using the TaskInstance as a ParamProvider. */
  virtual void triggerWithinTaskInstance(EventSubscription subscription,
                                         TaskInstance context) const;
  virtual QString targetName() const;
};

#endif // ACTION_P_H
