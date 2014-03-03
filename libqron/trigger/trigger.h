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
#ifndef TRIGGER_H
#define TRIGGER_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include "config/calendar.h"
#include "util/paramset.h"

class TriggerData;

class LIBQRONSHARED_EXPORT Trigger {
protected:
  QSharedDataPointer<TriggerData> d;

public:
  Trigger();
  Trigger(const Trigger &);
  Trigger &operator=(const Trigger &);
  ~Trigger();
  /** Trigger expression as it was initialy given.
   * e.g. "1/10 * * * * *", "noticename" */
  QString expression() const;
  /** Trigger expression in a canonical/unique form.
   * e.g. "1,11,21,31,41,51 * * * * *" */
  QString canonicalExpression() const;
  /** e.g. "(1/10 * * * * *)" or "^noticename" */
  QString humanReadableExpression() const;
  /** e.g. "[(calendar foo)](1/10 * * * * *)",
   * "[(calendar(include ..2013-1-1))]^noticename" */
  QString humanReadableExpressionWithCalendar() const;
  /** Trigger contains usable, not null or empty, data. */
  bool isValid() const;
  //void setCalendar(Calendar calendar);
  Calendar calendar() const;
  ParamSet overridingParams() const;

protected:
  Trigger(TriggerData *data);
  /** Load config element common to all trigger types
   * @return false on error */
  bool loadConfig(PfNode node, QHash<QString,Calendar> namedCalendars);
};

#endif // TRIGGER_H
