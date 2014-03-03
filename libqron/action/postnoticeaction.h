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
#ifndef POSTNOTICEACTION_H
#define POSTNOTICEACTION_H

#include "action.h"
#include <QString>
#include "util/paramset.h"

class PostNoticeActionData;
class Scheduler;

/** Action posting a notice. */
class LIBQRONSHARED_EXPORT PostNoticeAction : public Action {
public:
  explicit PostNoticeAction(Scheduler *scheduler = 0,
                            QString notice = QString(),
                            ParamSet params = ParamSet());
  PostNoticeAction(const PostNoticeAction &);
  ~PostNoticeAction();
};

#endif // POSTNOTICEACTION_H
