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
#ifndef EVENTSUBSCRIPTION_H
#define EVENTSUBSCRIPTION_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include "pf/pfnode.h"
#include "util/paramset.h"
#include <QStringList>

class EventSubscriptionData;
class TaskInstance;
class Task;
class Action;
class Scheduler;

/** Subscription to a given event (e.g. onsuccess) for one or several actions
 * (e.g. emitalert) with an optionnal filter and optionnal event subscription
 * parameters. */
class LIBQRONSHARED_EXPORT EventSubscription {
  QSharedDataPointer<EventSubscriptionData> d;

public:
  EventSubscription();
  EventSubscription(QString subscriberName, QString eventName, Action action);
  EventSubscription(QString subscriberName, QString eventName,
                    QList<Action> actions);
  /** Parse configuration fragment. */
  EventSubscription(QString subscriberName, PfNode node, Scheduler *scheduler,
                    QStringList ignoredChildren = QStringList());
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
   * e.g. taskid for a task, groupid for a group, fqsn for a step, * for global
   * subscriptions. */
  QString subscriberName() const;
  void setSubscriberName(QString name);
  /** Trigger actions if context complies with filter conditions.
   * Use this method if the event occured in the context of a task.
   * @param eventContext will get taskContext::params() as parent before being
   * passed to Action::trigger() */
  void triggerActions(ParamSet eventPrams, TaskInstance taskContext) const;
  /** Syntaxic sugar */
  void triggerActions(TaskInstance taskContext) const;
  /** Syntaxic sugar */
  void triggerActions(ParamSet eventPrams) const;
  /** Syntaxic sugar */
  void triggerActions() const;
  static QStringList toStringList(QList<EventSubscription> list);
  QStringList toStringList() const;
  PfNode toPfNode() const;
  /** Build target steps local ids for subscribed actions of relevant types
   * (StepAction and EndAction). */
  QStringList workflowTargetsLocalIds() const;
};

#endif // EVENTSUBSCRIPTION_H
