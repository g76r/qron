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
#include "postnoticeaction.h"
#include "action_p.h"
#include "config/configutils.h"

class PostNoticeActionData : public ActionData {
public:
  QString _notice;
  ParamSet _params;
  PostNoticeActionData(Scheduler *scheduler = 0, QString notice = QString(),
                       ParamSet params = ParamSet())
    : ActionData(scheduler), _notice(notice), _params(params) { }
  QString toString() const {
    return "^"+_notice;
  }
  QString actionType() const {
    return "postnotice";
  }
  void trigger(EventSubscription subscription,
               const ParamsProvider *context) const {
    Q_UNUSED(subscription)
    if (_scheduler) {
      // LATER do not use _params but a modified _params with subscription's params as parent
      ParamSet noticeParams;
      foreach (QString key, _params.keys())
        noticeParams.setValue(key, _params.value(key, true, context));
      _scheduler.data()
          ->postNotice(_params.evaluate(_notice, context), noticeParams);
    }
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
