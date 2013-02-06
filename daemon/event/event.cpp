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
#include "event_p.h"
#include <QSharedData>
#include "log/log.h"

Event::Event() : d(new EventData) {
}

Event::Event(const Event &rhs) : d(rhs.d) {
}

Event::Event(EventData *data) : d(data) {
}

Event &Event::operator=(const Event &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

Event::~Event() {
}

void Event::trigger(const ParamsProvider *context) const {
  if (d)
    d->trigger(context);
}

QString EventData::toString() const {
  return "Event";
}

void EventData::trigger(const ParamsProvider *context) const {
  Q_UNUSED(context)
  Log::warning() << "EventData::trigger() called whereas it should never";
}

QString Event::toString() const {
  return d ? d->toString() : QString();
}

QStringList Event::toStringList(const QList<Event> list) {
  QStringList sl;
  foreach (const Event e, list)
    sl.append(e.toString());
  return sl;
}
