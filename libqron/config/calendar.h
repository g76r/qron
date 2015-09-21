/* Copyright 2013-2015 Hallowyn and others.
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
#ifndef CALENDAR_H
#define CALENDAR_H

#include "libqron_global.h"
#include "modelview/shareduiitem.h"
#include <QDate>
#include "pf/pfnode.h"

class CalendarData;

/** Definition of days when a event can occur or not.
 * Used with CronTrigger to handle special arbitrary days such as public
 * holidays.
 * @see CronTrigger */
class LIBQRONSHARED_EXPORT Calendar : public SharedUiItem {

public:
  Calendar() : SharedUiItem() { }
  Calendar(const Calendar &other) : SharedUiItem(other) { }
  Calendar(PfNode node);
  Calendar &operator=(const Calendar &other) {
    SharedUiItem::operator=(other); return *this; }
  Calendar &append(QDate begin, QDate end, bool include);
  inline Calendar &append(QDate date, bool include) {
    return append(date, date, include); }
  inline Calendar &append(bool include) {
    return append(QDate(), QDate(), include); }
  bool isIncluded(QDate date) const;
  /** e.g. (calendar name) or (calendar(include 2014-01-01..2018-12-31)) */
  PfNode toPfNode(bool useNameOnlyIfSet = false) const;
  /** Enumerate rules in a human readable fashion
    * e.g. "include 2014-01-01..2018-12-31, exclude 2014-04-01" */
  QString toCommaSeparatedRulesString() const;
  QString name() const;

private:
  const CalendarData *data() const {
    return (const CalendarData*)SharedUiItem::data(); }
  CalendarData *data();
};

#endif // CALENDAR_H
