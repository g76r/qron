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
#include "raisealertevent.h"
#include "event_p.h"

class RaiseAlertEventData : public EventData {
public:
  QString _alert;
  RaiseAlertEventData(Scheduler *scheduler = 0, const QString alert = QString())
    : EventData(scheduler), _alert(alert) { }
  void trigger(const ParamsProvider *context) const {
    if (_scheduler)
      _scheduler.data()->alerter()
          ->raiseAlert(ParamSet().evaluate(_alert, context));
  }
  QString toString() const {
    return "!+"+_alert;
  }
};

RaiseAlertEvent::RaiseAlertEvent(Scheduler *scheduler, const QString alert)
  : Event(new RaiseAlertEventData(scheduler, alert)) {
}

RaiseAlertEvent::RaiseAlertEvent(const RaiseAlertEvent &rhs) : Event(rhs) {
}

RaiseAlertEvent::~RaiseAlertEvent() {
}
