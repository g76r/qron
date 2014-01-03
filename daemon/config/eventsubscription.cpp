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
#include "eventsubscription.h"
#include <QSharedData>
#include "task.h"
#include "sched/taskinstance.h"
#include "action/action.h"
#include "configutils.h"

class EventSubscriptionData : public QSharedData {
public:
  QString _subscriberName, _eventName;
  QList<Action> _actions;
  ParamSet _params;
  EventSubscriptionData(QString subscriberName = QString(),
                        QString eventName = QString(),
                        ParamSet params = ParamSet())
    : _subscriberName(subscriberName), _eventName(eventName),
      _params(params) { }
};

EventSubscription::EventSubscription() {
}

EventSubscription::EventSubscription(
    QString subscriberName, PfNode node, Scheduler *scheduler)
  : d(new EventSubscriptionData(subscriberName)) {
  // LATER implement at less regexp filter
  // LATER support for non-text/non-regexp filters e.g. "onstatus >=3"
  // LATER support for (ifincalendar) and other structured filters
  d->_eventName = node.name();
  ConfigUtils::loadParamSet(node, &d->_params);
  foreach (PfNode child, node.children()) {
    Action a = Action::createAction(child, scheduler);
    if (!a.isNull())
      d->_actions.append(a);
  }
}

EventSubscription::EventSubscription(
    QString subscriberName, QString eventName, Action action)
  : d(new EventSubscriptionData(subscriberName, eventName)) {
  d->_actions.append(action);
}

EventSubscription::EventSubscription(
    QString subscriberName, QString eventName, QList<Action> actions)
  : d(new EventSubscriptionData(subscriberName, eventName)) {
  d->_actions = actions;
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

void EventSubscription::triggerActions(
    ParamSet eventContext, TaskInstance taskContext) const {
  // LATER implement filters
  // inheritage will be: eventContext > EventSubscription > taskContext
  ParamSet subscriptionParams = d->_params;
  subscriptionParams.setParent(taskContext.params());
  eventContext.setParent(subscriptionParams);
  //Log::fatal() << "EventSubscription::triggerActions " << eventContext << " "
  //             << taskContext.id();
  foreach (Action a, d->_actions)
    a.trigger(*this, eventContext, taskContext);
}

void EventSubscription::triggerActions(TaskInstance taskContext) const {
  // LATER implement filters
  // inheritage will be: EventSubscription > taskContext
  ParamSet eventContext = d->_params;
  eventContext.setParent(taskContext.params());
  //Log::fatal() << "EventSubscription::triggerActions " << taskContext.id();
  foreach (Action a, d->_actions)
    a.trigger(*this, eventContext, taskContext);
}

void EventSubscription::triggerActions(ParamSet eventContext) const {
  // LATER implement filters
  // inheritage will be: eventContext > EventSubscription
  eventContext.setParent(d->_params);
  //Log::fatal() << "EventSubscription::triggerActions " << eventContext;
  foreach (Action a, d->_actions)
    a.trigger(*this, eventContext);
}

void EventSubscription::triggerActions() const {
  // LATER implement filters
  //Log::fatal() << "EventSubscription::triggerActions ";
  foreach (Action a, d->_actions)
    a.trigger(*this, d->_params);
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
  // LATER implement filters
  return d ? d->_eventName : QString();
}

QList<Action> EventSubscription::actions() const {
  return d ? d->_actions : QList<Action>();
}

QString EventSubscription::subscriberName() const {
  return d ? d->_subscriberName : QString();
}

ParamSet EventSubscription::params() const {
  return d ? d->_params : ParamSet();
}
