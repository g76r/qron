/* Copyright 2012-2014 Hallowyn and others.
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
#include "taskgroup.h"
#include "util/paramset.h"
#include <QString>
#include "task.h"
#include "pf/pfnode.h"
#include <QtDebug>
#include "config/eventsubscription.h"
#include <QPointer>
#include "config/configutils.h"
#include "sched/taskinstance.h"

class TaskGroupData : public QSharedData {
public:
  QString _id, _label;
  ParamSet _params, _setenv, _unsetenv;
  QList<EventSubscription> _onstart, _onsuccess, _onfailure;
};

TaskGroup::TaskGroup() : d(new TaskGroupData) {
}

TaskGroup::TaskGroup(const TaskGroup &other) : d(other.d) {
}

TaskGroup::TaskGroup(PfNode node, ParamSet parentParamSet,
                     ParamSet parentSetenv, ParamSet parentUnsetenv,
                     Scheduler *scheduler) {
  TaskGroupData *tgd = new TaskGroupData;
  tgd->_id = ConfigUtils::sanitizeId(node.contentAsString(), true);
  tgd->_label = node.attribute("label", tgd->_id);
  tgd->_params.setParent(parentParamSet);
  ConfigUtils::loadParamSet(node, &tgd->_params);
  tgd->_setenv.setParent(parentSetenv);
  ConfigUtils::loadSetenv(node, &tgd->_setenv);
  ConfigUtils::loadUnsetenv(node, &tgd->_unsetenv);
  tgd->_unsetenv.setParent(parentUnsetenv);
  ConfigUtils::loadEventSubscription(node, "onstart", tgd->_id,
                                     &tgd->_onstart, scheduler);
  ConfigUtils::loadEventSubscription(node, "onsuccess", tgd->_id,
                                     &tgd->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", tgd->_id,
                                     &tgd->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfailure", tgd->_id,
                                     &tgd->_onfailure, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", tgd->_id,
                                     &tgd->_onfailure, scheduler);
  d = tgd;
}

TaskGroup::~TaskGroup() {
}

TaskGroup &TaskGroup::operator=(const TaskGroup &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

QString TaskGroup::id() const {
  return d->_id;
}

QString TaskGroup::label() const {
  return d->_label;
}

ParamSet TaskGroup::params() const {
  return d->_params;
}

bool TaskGroup::isNull() const {
  return d->_id.isNull();
}

QDebug operator<<(QDebug dbg, const TaskGroup &taskGroup) {
  dbg.nospace() << taskGroup.id();
  return dbg.space();
}

void TaskGroup::triggerStartEvents(TaskInstance instance) const {
  // LATER trigger events in parent group first
  if (d)
    foreach (EventSubscription sub, d->_onstart)
      sub.triggerActions(instance);
}

void TaskGroup::triggerSuccessEvents(TaskInstance instance) const {
  if (d)
    foreach (EventSubscription sub, d->_onsuccess)
      sub.triggerActions(instance);
}

void TaskGroup::triggerFailureEvents(TaskInstance instance) const {
  if (d)
    foreach (EventSubscription sub, d->_onfailure)
      sub.triggerActions(instance);
}

QList<EventSubscription> TaskGroup::onstartEventSubscriptions() const {
  return d->_onstart;
}

QList<EventSubscription> TaskGroup::onsuccessEventSubscriptions() const {
  return d->_onsuccess;
}

QList<EventSubscription> TaskGroup::onfailureEventSubscriptions() const {
  return d->_onfailure;
}

ParamSet TaskGroup::setenv() const {
  return d ? d->_setenv : ParamSet();
}

ParamSet TaskGroup::unsetenv() const {
  return d ? d->_unsetenv : ParamSet();
}

QList<EventSubscription> TaskGroup::allEventSubscriptions() const {
  // LATER avoid creating the collection at every call
  return d ? d->_onstart+d->_onsuccess+d->_onfailure : QList<EventSubscription>();
}
