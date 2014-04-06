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

static int _uiTaskGroupColumnToTask[] =
{
  0, 2, 7, 14, 15,
  16, 20, 21, 22
};

static int _uiTaskColumnToTaskGroup[] =
{
  0, -1, 1, -1, -1,
  -1, 2, -1, -1, -1,
  -1, -1, -1, 3, 4,
  5, -1, -1, -1, -1,
  6, 7, 8
};

int TaskGroup::uiTaskGroupColumnToTask(int taskGroupColumn) {
  if (taskGroupColumn >= 0
      && taskGroupColumn < (int)sizeof(_uiTaskGroupColumnToTask))
    return _uiTaskGroupColumnToTask[taskGroupColumn];
  return -1;
}

int TaskGroup::uiTaskColumnToTaskGroup(int taskColumn) {
  if (taskColumn >= 0
      && taskColumn < (int)sizeof(_uiTaskColumnToTaskGroup))
    return _uiTaskColumnToTaskGroup[taskColumn];
  return -1;
}

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Label",
  "Parameters",
  "On start",
  "On success",
  "On failure", // 5
  "System environment",
  "Setenv",
  "Unsetenv"
};

class TaskGroupData : public SharedUiItemData {
public:
  QString _id, _label;
  ParamSet _params, _setenv, _unsetenv;
  QList<EventSubscription> _onstart, _onsuccess, _onfailure;
  QVariant uiData(int section, int role) const;
  QString id() const { return _id; }
};

TaskGroup::TaskGroup() {
}

TaskGroup::TaskGroup(const TaskGroup &other) : SharedUiItem(other) {
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
  setData(tgd);
}

TaskGroup::TaskGroup(QString id) {
  TaskGroupData *tgd = new TaskGroupData;
  tgd->_id = ConfigUtils::sanitizeId(id, true);
  setData(tgd);
}

TaskGroup::~TaskGroup() {
}

QString TaskGroup::parentGroupId(QString groupId) {
  int i = groupId.lastIndexOf('.');
  return (i >= 0) ? groupId.left(i) : QString();
}

QString TaskGroup::label() const {
  return !isNull() ? tgd()->_label : QString();
}

ParamSet TaskGroup::params() const {
  return !isNull() ? tgd()->_params : ParamSet();
}

QDebug operator<<(QDebug dbg, const TaskGroup &taskGroup) {
  dbg.nospace() << taskGroup.id();
  return dbg.space();
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
      return _id;
    case 1:
      return _label;
    case 2:
      return _params.toString(false, false);
    case 3:
      return EventSubscription::toStringList(_onstart).join("\n");
    case 4:
      return EventSubscription::toStringList(_onsuccess).join("\n");
    case 5:
      return EventSubscription::toStringList(_onfailure).join("\n");
    case 6: { // LATER factorize code for cols 6,7,8 with Task's 20,21,22
      QString env;
      foreach(const QString key, _setenv.keys(false))
        env.append(key).append('=').append(_setenv.rawValue(key)).append(' ');
      foreach(const QString key, _unsetenv.keys(false))
        env.append('-').append(key).append(' ');
      env.chop(1);
      return env;
    }
    case 7: {
      QString env;
      foreach(const QString key, _setenv.keys(false))
        env.append(key).append('=').append(_setenv.rawValue(key)).append(' ');
      env.chop(1);
      return env;
    }
    case 8: {
      QString env;
      foreach(const QString key, _unsetenv.keys(false))
        env.append(key).append(' ');
      env.chop(1);
      return env;
    }
    }
    break;
  default:
    ;
  }
  return QVariant();
}

QVariant TaskGroup::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int TaskGroup::uiDataCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

TaskGroupData *TaskGroup::tgd() {
  detach<TaskGroupData>();
  return (TaskGroupData*)constData();
}
