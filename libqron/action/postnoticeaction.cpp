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
#include "postnoticeaction.h"
#include "action_p.h"
#include "config/configutils.h"

class PostNoticeActionData : public ActionData {
public:
  QString _notice;
  ParamSet _noticeParams;
  PostNoticeActionData(Scheduler *scheduler = 0, QString notice = QString(),
                       ParamSet params = ParamSet())
    : ActionData(scheduler), _notice(notice), _noticeParams(params) { }
  QString toString() const {
    return "^"+_notice;
  }
  QString actionType() const {
    return "postnotice";
  }
  void trigger(EventSubscription subscription, ParamSet eventContext,
               TaskInstance taskContext) const {
    // we must implement trigger(,,TaskInstance) rather than trigger(,) because
    // even though we do not directly use TaskInstance, we want that
    // EventSubscription::triggerActions() use task params as eventContext
    // grandparent
    Q_UNUSED(subscription)
    if (_scheduler) {
      ParamSet noticeParams;
      foreach (QString key, _noticeParams.keys())
        noticeParams.setValue(
              key, eventContext.evaluate(_noticeParams.value(key),
                                         &taskContext));
      _scheduler.data()
          ->postNotice(eventContext.evaluate(_notice, &taskContext),
                       noticeParams);
    }
  }
  void trigger(EventSubscription subscription, ParamSet eventContext) const {
    trigger(subscription, eventContext, TaskInstance());
  }
  QString targetName() const {
    return _notice;
  }
};

PostNoticeAction::PostNoticeAction(
    Scheduler *scheduler, QString notice, ParamSet params)
  : Action(new PostNoticeActionData(scheduler, notice, params)) {
}

PostNoticeAction::PostNoticeAction(const PostNoticeAction &rhs) : Action(rhs) {
}

PostNoticeAction::~PostNoticeAction() {
}
