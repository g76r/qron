/* Copyright 2013-2015 Hallowyn and others.
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
#include "eventsubscription.h"
#include <QSharedData>
#include "task.h"
#include "sched/taskinstance.h"
#include "action/action.h"
#include "configutils.h"
#include "sched/scheduler.h"

class EventSubscriptionData : public QSharedData {
public:
  QString _subscriberName, _eventName;
  QList<Action> _actions;
  ParamSet _globalParams;
  QStringList _commentsList;
  EventSubscriptionData(QString subscriberName = QString(),
                        QString eventName = QString(),
                        ParamSet globalParams = ParamSet())
    : _subscriberName(subscriberName), _eventName(eventName),
      _globalParams(globalParams) { }
};

EventSubscription::EventSubscription() {
}

EventSubscription::EventSubscription(
    QString subscriberName, PfNode node, Scheduler *scheduler,
    QStringList ignoredChildren)
  : d(new EventSubscriptionData(subscriberName)) {
  // LATER implement at less regexp filter
  // LATER support for non-text/non-regexp filters e.g. "onstatus >=3"
  // LATER support for (calendar) and other structured filters
  d->_eventName = node.name();
  if (scheduler)
    d->_globalParams = scheduler->globalParams();
  foreach (PfNode child, node.children()) {
    if (ignoredChildren.contains(child.name()) || child.isComment())
      continue;
    Action a = Action::createAction(child, scheduler, d->_eventName);
    if (!a.isNull())
      d->_actions.append(a);
  }
  ConfigUtils::loadComments(node, &d->_commentsList, 0);
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

void EventSubscription::triggerActions(
    ParamSet eventParams, TaskInstance taskContext) const {
  if (!d)
    return;
  if (eventParams.parent().isNull())
    eventParams.setParent(d->_globalParams);
  // LATER implement filters
  // inheritage will be: eventContext > taskContext > ...
  //Log::fatal() << "EventSubscription::triggerActions " << eventContext << " "
  //             << taskContext.id();
  foreach (Action a, d->_actions)
    a.trigger(*this, eventParams, taskContext);
}

void EventSubscription::triggerActions(TaskInstance taskContext) const {
  triggerActions(ParamSet(), taskContext);
}

void EventSubscription::triggerActions(ParamSet eventParams) const {
  triggerActions(eventParams, TaskInstance());
}

void EventSubscription::triggerActions() const {
  triggerActions(ParamSet(), TaskInstance());
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

void EventSubscription::setSubscriberName(QString name) {
  if (d)
    d->_subscriberName = name;
}

PfNode EventSubscription::toPfNode() const {
  if (!d)
    return PfNode();
  PfNode node(d->_eventName);
  ConfigUtils::writeComments(&node, d->_commentsList);
  foreach(const Action &action, d->_actions)
    node.appendChild(action.toPfNode());
  return node;
}

QStringList EventSubscription::workflowTargetsLocalIds() const {
  QSet<QString> localIds;
  foreach (Action action, actions()) {
    if (action.actionType() == "step"
        || action.actionType() == "end") {
      localIds << action.targetName();
    }
  }
  return localIds.toList();
}
