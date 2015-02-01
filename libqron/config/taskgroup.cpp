/* Copyright 2012-2015 Hallowyn and others.
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
#include "task_p.h"
#include "ui/qronuiutils.h"

class TaskGroupData : public SharedUiItemData {
public:
  QString _id, _label;
  ParamSet _params, _setenv, _unsetenv;
  QList<EventSubscription> _onstart, _onsuccess, _onfailure;
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const;
  int uiSectionCount() const;
  QString id() const { return _id; }
  //void setId(QString id) { _id = id; }
  QString idQualifier() const { return "taskgroup"; }
};

TaskGroup::TaskGroup() {
}

TaskGroup::TaskGroup(const TaskGroup &other) : SharedUiItem(other) {
}

TaskGroup::TaskGroup(PfNode node, ParamSet parentParamSet,
                     ParamSet parentSetenv, ParamSet parentUnsetenv,
                     Scheduler *scheduler) {
  TaskGroupData *tgd = new TaskGroupData;
  tgd->_id = ConfigUtils::sanitizeId(node.contentAsString(),
                                     ConfigUtils::GroupId);
  tgd->_label = node.attribute("label");
  tgd->_params.setParent(parentParamSet);
  ConfigUtils::loadParamSet(node, &tgd->_params, "param");
  tgd->_setenv.setParent(parentSetenv);
  ConfigUtils::loadParamSet(node, &tgd->_setenv, "setenv");
  ConfigUtils::loadFlagSet(node, &tgd->_unsetenv, "unsetenv");
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
  setData(tgd);
}

TaskGroup::TaskGroup(QString id) {
  TaskGroupData *tgd = new TaskGroupData;
  tgd->_id = ConfigUtils::sanitizeId(id, ConfigUtils::GroupId);
  setData(tgd);
}

QString TaskGroup::parentGroupId(QString groupId) {
  int i = groupId.lastIndexOf('.');
  return (i >= 0) ? groupId.left(i) : QString();
}

QString TaskGroup::label() const {
  return !isNull() ? (tgd()->_label.isNull() ? tgd()->_id : tgd()->_label)
                   : QString();
}

ParamSet TaskGroup::params() const {
  return !isNull() ? tgd()->_params : ParamSet();
}

void TaskGroup::triggerStartEvents(TaskInstance instance) const {
  // LATER trigger events in parent group first
  if (!isNull())
    foreach (EventSubscription sub, tgd()->_onstart)
      sub.triggerActions(instance);
}

void TaskGroup::triggerSuccessEvents(TaskInstance instance) const {
  if (!isNull())
    foreach (EventSubscription sub, tgd()->_onsuccess)
      sub.triggerActions(instance);
}

void TaskGroup::triggerFailureEvents(TaskInstance instance) const {
  if (!isNull())
    foreach (EventSubscription sub, tgd()->_onfailure)
      sub.triggerActions(instance);
}

QList<EventSubscription> TaskGroup::onstartEventSubscriptions() const {
  return !isNull() ? tgd()->_onstart : QList<EventSubscription>();
}

QList<EventSubscription> TaskGroup::onsuccessEventSubscriptions() const {
  return !isNull() ? tgd()->_onsuccess : QList<EventSubscription>();
}

QList<EventSubscription> TaskGroup::onfailureEventSubscriptions() const {
  return !isNull() ? tgd()->_onfailure : QList<EventSubscription>();
}

ParamSet TaskGroup::setenv() const {
  return !isNull() ? tgd()->_setenv : ParamSet();
}

ParamSet TaskGroup::unsetenv() const {
  return !isNull() ? tgd()->_unsetenv : ParamSet();
}

QList<EventSubscription> TaskGroup::allEventSubscriptions() const {
  // LATER avoid creating the collection at every call
  return !isNull() ? tgd()->_onstart + tgd()->_onsuccess + tgd()->_onfailure
                   : QList<EventSubscription>();
}

QVariant TaskGroupData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
    switch(section) {
    case 0:
    case 11:
      return _id;
    case 1:
      return TaskGroup::parentGroupId(_id);
    case 2:
      return _label.isNull() ? _id : _label;
    case 7:
      return _params.toString(false, false);
    case 14:
      return EventSubscription::toStringList(_onstart).join("\n");
    case 15:
      return EventSubscription::toStringList(_onsuccess).join("\n");
    case 16:
      return EventSubscription::toStringList(_onfailure).join("\n");
    case 20:
      return QronUiUtils::sysenvAsString(_setenv, _unsetenv);
    case 21:
      return QronUiUtils::paramsAsString(_setenv);
    case 22:
      return QronUiUtils::paramsKeysAsString(_unsetenv);
    }
    break;
  default:
    ;
  }
  return QVariant();
}

QVariant TaskGroupData::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int TaskGroupData::uiSectionCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

TaskGroupData *TaskGroup::tgd() {
  detach<TaskGroupData>();
  return (TaskGroupData*)constData();
}

PfNode TaskGroup::toPfNode() const {
  if (!tgd())
    return PfNode();
  PfNode node("taskgroup", tgd()->id());
  if (!tgd()->_label.isNull())
    node.setAttribute("label", tgd()->_label);
  ConfigUtils::writeParamSet(&node, tgd()->_params, "param");
  ConfigUtils::writeParamSet(&node, tgd()->_setenv, "setenv");
  ConfigUtils::writeFlagSet(&node, tgd()->_unsetenv, "unsetenv");
  ConfigUtils::writeEventSubscriptions(&node, tgd()->_onstart);
  ConfigUtils::writeEventSubscriptions(&node, tgd()->_onsuccess);
  ConfigUtils::writeEventSubscriptions(&node, tgd()->_onfailure,
                                       QStringList("onfinish"));
  return node;
}
