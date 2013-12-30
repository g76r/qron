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
#include "eventsubscription.h"
#include <QSharedData>
#include "task.h"
#include "sched/taskinstance.h"
#include "action/action.h"

class EventSubscriptionData : public QSharedData {
public:
  QString _eventName;
  QList<Action> _actions;
};

EventSubscription::EventSubscription() {
}

EventSubscription::EventSubscription(PfNode node, Scheduler *scheduler)
  : d(new EventSubscriptionData) {
  d->_eventName = node.name();
  // TODO load filter
  // LATER support for non-text/non-regexp filters e.g. "onstatus >=3"
  foreach (PfNode child, node.children()) {
    // LATER support for (ifincalendar) and other structured filters
    Action a = Action::createAction(child, scheduler);
    if (!a.isNull())
      d->_actions.append(a);
  }
}

EventSubscription::~EventSubscription() {
}

EventSubscription::EventSubscription(const EventSubscription &rhs) : d(rhs.d) {
}

EventSubscription &EventSubscription::operator=(const EventSubscription &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

class EventContext : public ParamsProvider {
public:
  EventContext() { }
  QVariant paramValue(const QString key, const QVariant defaultValue) const {
    Q_UNUSED(key)
    return defaultValue;
  }
};

void EventSubscription::triggerActions(TaskInstance context) const {
  // TODO implement filters and EventContext
  foreach (Action a, d->_actions)
    a.trigger(&context);
}

void EventSubscription::triggerActions(const ParamsProvider *context) const {
  // TODO implement filters and EventContext
  foreach (Action a, d->_actions)
    a.trigger(context);
}

QStringList EventSubscription::toStringList(QList<EventSubscription> list) {
  QStringList l;
  foreach (EventSubscription sub, list)
    l.append(sub.toStringList());
  return l;
}

QStringList EventSubscription::toStringList() const {
  return Action::toStringList(actions());
}

QString EventSubscription::eventName() const {
  return d ? d->_eventName : QString();
}

QString EventSubscription::humanReadableCause() const {
  // TODO implement filters
  return d ? d->_eventName : QString();
}

QList<Action> EventSubscription::actions() const {
  return d ? d->_actions : QList<Action>();
}
