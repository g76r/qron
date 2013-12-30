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
  PostNoticeActionData(Scheduler *scheduler = 0, QString notice = QString())
    : ActionData(scheduler), _notice(notice) { }
  QString toString() const {
    return "^"+_notice;
  }
  QString actionType() const {
    return "postnotice";
  }
  void trigger(const ParamsProvider *context) const {
    if (_scheduler)
      _scheduler.data()->postNotice(ParamSet().evaluate(_notice, context));
  }
  QString targetName() const {
    return _notice;
  }
};

PostNoticeAction::PostNoticeAction(Scheduler *scheduler, QString notice)
  : Action(new PostNoticeActionData(scheduler, notice)) {
}

PostNoticeAction::PostNoticeAction(const PostNoticeAction &rhs) : Action(rhs) {
}

PostNoticeAction::~PostNoticeAction() {
}
