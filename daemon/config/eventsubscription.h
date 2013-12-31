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
#ifndef EVENTSUBSCRIPTION_H
#define EVENTSUBSCRIPTION_H

#include <QSharedDataPointer>
#include "pf/pfnode.h"
#include "util/paramsprovider.h"

class EventSubscriptionData;
class TaskInstance;
class Task;
class Action;
class Scheduler;

/** Subscription to a given event (e.g. onsuccess) for one or several actions
 * (e.g. emitalert) with an optionnal filter and optionnal event subscription
 * parameters. */
class EventSubscription {
  QSharedDataPointer<EventSubscriptionData> d;

public:
  EventSubscription();
  EventSubscription(QString subscriberName, QString eventName, Action action);
  EventSubscription(QString subscriberName, QString eventName,
                    QList<Action> actions);
  /** Parse configuration fragment. */
  EventSubscription(QString subscriberName, PfNode node, Scheduler *scheduler);
  EventSubscription(const EventSubscription &);
  EventSubscription &operator=(const EventSubscription &);
  ~EventSubscription();
  bool isNull() const { return !d; }
  QList<Action> actions() const;
  /** Name of the matched event. e.g. "onstart" "onnotice" "onstatus" */
  QString eventName() const;
  /** Full description of event matched with applicable filter.
   * e.g. "onstart" "onnotice /foo.*bar/" "onstatus >= 3" */
  QString humanReadableCause() const;
  /** Name identifiying the subscriber.
   * e.g. fqtn for a task, groupid for a group, fqsn for a step, * for global
   * subscriptions. */
  QString subscriberName() const;
  /** Trigger actions if context complies with filter conditions.
   * Use this method if the event occured in the context of a task. */
  void triggerActions(TaskInstance context) const;
  /** Trigger actions if context complies with filter conditions.
   * Use this method if the event occured outside the context of a task. */
  void triggerActions(const ParamsProvider *context) const;
  static QStringList toStringList(QList<EventSubscription> list);
  QStringList toStringList() const;
};

#endif // EVENTSUBSCRIPTION_H
