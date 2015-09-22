/* Copyright 2013-2014 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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
#ifndef EMITALERTACTION_H
#define EMITALERTACTION_H

#include "action.h"

class EmitAlertActionData;
class Scheduler;

/** Action emitting an alert. */
class LIBQRONSHARED_EXPORT EmitAlertAction : public Action {
public:
  explicit EmitAlertAction(Scheduler *scheduler = 0, PfNode node = PfNode());
  EmitAlertAction(const EmitAlertAction &);
  ~EmitAlertAction();
};

#endif // EMITALERTACTION_H
