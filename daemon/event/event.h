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
#ifndef EVENT_H
#define EVENT_H

#include <QSharedDataPointer>
#include "util/paramsprovider.h"
#include <QStringList>

class EventData;

class Event {
protected:
  QSharedDataPointer<EventData> d;

public:
  Event();
  Event(const Event &);
  Event &operator=(const Event &);
  ~Event();
  void trigger(const ParamsProvider *context) const;
  /** Human readable description of event */
  QString toString() const;
  /** Type of event for programmatic test, e.g. "postnotice", "setflag" */
  QString eventType() const;
  static QStringList toStringList(QList<Event> list);

protected:
  explicit Event(EventData *data);
};

#endif // EVENT_H
