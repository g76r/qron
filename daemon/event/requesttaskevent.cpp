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
#include "requesttaskevent.h"
#include "event_p.h"

class RequestTaskEventData : public EventData {
public:
  QString _fqtn;
  ParamSet _params;
  bool _force;
  RequestTaskEventData(Scheduler *scheduler = 0, const QString fqtn = QString(),
                       ParamSet params = ParamSet(), bool force = false)
    : EventData(scheduler), _fqtn(fqtn), _params(params), _force(force) { }
  void trigger(const ParamsProvider *context) const {
    if (_scheduler) {
      QString fqtn = context ? context->paramValue("!fqtn") : QString();
      QString id = context ? context->paramValue("!taskrequestid") : QString();
      Log::log(fqtn.isNull() ? Log::Debug : Log::Info, fqtn, id.toLongLong())
          << "requesttask event requesting execution of task " << _fqtn;
      _scheduler.data()->asyncRequestTask(_params.evaluate(_fqtn, context),
                                          _params, _force);
      // LATER if requestTask returns the TaskRequest object, we can track child taskrequestid
      // LATER this special case should be logged to a special data model to enable drawing parent-child diagrams
    }
  }
  QString toString() const {
    return "*" + _fqtn;
  }
};

RequestTaskEvent::RequestTaskEvent(Scheduler *scheduler, const QString fqtn,
                                   ParamSet params, bool force)
  : Event(new RequestTaskEventData(scheduler, fqtn, params, force)) {
}

RequestTaskEvent::RequestTaskEvent(const RequestTaskEvent &rhs) : Event(rhs) {
}

RequestTaskEvent::~RequestTaskEvent() {
}
