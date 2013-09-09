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
#include "setflagevent.h"
#include "event_p.h"
#include <QWeakPointer>

class SetFlagEventData : public EventData {
public:
  QString _flag;
  SetFlagEventData(Scheduler *scheduler, QString flag)
    : EventData(scheduler), _flag(flag) { }
  void trigger(const ParamsProvider *context) const {
    if (_scheduler)
      _scheduler.data()->setFlag(ParamSet().evaluate(_flag, context));
  }
  QString toString() const {
    return "+" + _flag;
  }
  QString eventType() const {
    return "setflag";
  }
};

SetFlagEvent::SetFlagEvent(Scheduler *scheduler, QString flag)
  : Event(new SetFlagEventData(scheduler, flag)) {
}

SetFlagEvent::SetFlagEvent(const SetFlagEvent &rhs) : Event(rhs) {
}

SetFlagEvent::~SetFlagEvent() {
}
