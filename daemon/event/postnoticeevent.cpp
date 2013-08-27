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
#include "postnoticeevent.h"
#include "event_p.h"

class PostNoticeEventData : public EventData {
public:
  QString _notice;
  PostNoticeEventData(Scheduler *scheduler = 0,
                      const QString notice = QString())
    : EventData(scheduler), _notice(notice) { }
  QString toString() const {
    return "^"+_notice;
  }
  QString eventType() const {
    return "postnotice";
  }
  void trigger(const ParamsProvider *context) const {
    if (_scheduler)
      _scheduler.data()->postNotice(ParamSet().evaluate(_notice, context));
  }
};

PostNoticeEvent::PostNoticeEvent(Scheduler *scheduler, const QString notice)
  : Event(new PostNoticeEventData(scheduler, notice)) {
}

PostNoticeEvent::PostNoticeEvent(const PostNoticeEvent &rhs) : Event(rhs) {
}

PostNoticeEvent::~PostNoticeEvent() {
}
