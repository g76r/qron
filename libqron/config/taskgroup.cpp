/* Copyright 2012-2015 Hallowyn and others.
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
#include "modelview/shareduiitemdocumentmanager.h"

static QSet<QString> excludedDescendantsForComments {
  "onsuccess", "onfailure", "onfinish", "onstart", "ontrigger"
};

static QStringList excludeOnfinishSubscriptions { "onfinish" };

class TaskGroupData : public SharedUiItemData {
public:
  QString _id, _label;
  ParamSet _params, _setenv, _unsetenv;
  QList<EventSubscription> _onstart, _onsuccess, _onfailure;
  QStringList _commentsList;
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const;
  int uiSectionCount() const;
  QString id() const { return _id; }
  //void setId(QString id) { _id = id; }
  QString idQualifier() const { return "taskgroup"; }
  bool setUiData(int section, const QVariant &value, QString *errorString,
                 SharedUiItemDocumentTransaction *transaction, int role);
  Qt::ItemFlags uiFlags(int section) const;

};

TaskGroup::TaskGroup() {
}

TaskGroup::TaskGroup(const TaskGroup &other) : SharedUiItem(other) {
}

TaskGroup::TaskGroup(PfNode node, ParamSet parentParamSet,
                     ParamSet parentSetenv, ParamSet parentUnsetenv,
                     Scheduler *scheduler) {
  TaskGroupData *d = new TaskGroupData;
  d->_id = ConfigUtils::sanitizeId(node.contentAsString(),
                                     ConfigUtils::FullyQualifiedId);
  d->_label = node.attribute("label");
  d->_params.setParent(parentParamSet);
  ConfigUtils::loadParamSet(node, &d->_params, "param");
  d->_setenv.setParent(parentSetenv);
  ConfigUtils::loadParamSet(node, &d->_setenv, "setenv");
  ConfigUtils::loadFlagSet(node, &d->_unsetenv, "unsetenv");
  d->_unsetenv.setParent(parentUnsetenv);
  ConfigUtils::loadEventSubscription(node, "onstart", d->_id,
                                     &d->_onstart, scheduler);
  ConfigUtils::loadEventSubscription(node, "onsuccess", d->_id,
                                     &d->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", d->_id,
                                     &d->_onsuccess, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfailure", d->_id,
                                     &d->_onfailure, scheduler);
  ConfigUtils::loadEventSubscription(node, "onfinish", d->_id,
                                     &d->_onfailure, scheduler);
  ConfigUtils::loadComments(node, &d->_commentsList,
                            excludedDescendantsForComments);
  setData(d);
}

TaskGroup::TaskGroup(QString id) {
  TaskGroupData *d = new TaskGroupData;
  d->_id = ConfigUtils::sanitizeId(id, ConfigUtils::FullyQualifiedId);
  setData(d);
}

QString TaskGroup::parentGroupId(QString groupId) {
  int i = groupId.lastIndexOf('.');
  return (i >= 0) ? groupId.left(i) : QString();
}

QString TaskGroup::label() const {
  return !isNull() ? (data()->_label.isNull() ? data()->_id : data()->_label)
                   : QString();
}

ParamSet TaskGroup::params() const {
  return !isNull() ? data()->_params : ParamSet();
}

void TaskGroup::triggerStartEvents(TaskInstance instance) const {
  // LATER trigger events in parent group first
  if (!isNull())
    foreach (EventSubscription sub, data()->_onstart)
      sub.triggerActions(instance);
}

void TaskGroup::triggerSuccessEvents(TaskInstance instance) const {
  if (!isNull())
    foreach (EventSubscription sub, data()->_onsuccess)
      sub.triggerActions(instance);
}

void TaskGroup::triggerFailureEvents(TaskInstance instance) const {
  if (!isNull())
    foreach (EventSubscription sub, data()->_onfailure)
      sub.triggerActions(instance);
}

QList<EventSubscription> TaskGroup::onstartEventSubscriptions() const {
  return !isNull() ? data()->_onstart : QList<EventSubscription>();
}

QList<EventSubscription> TaskGroup::onsuccessEventSubscriptions() const {
  return !isNull() ? data()->_onsuccess : QList<EventSubscription>();
}

QList<EventSubscription> TaskGroup::onfailureEventSubscriptions() const {
  return !isNull() ? data()->_onfailure : QList<EventSubscription>();
}

ParamSet TaskGroup::setenv() const {
  return !isNull() ? data()->_setenv : ParamSet();
}

ParamSet TaskGroup::unsetenv() const {
  return !isNull() ? data()->_unsetenv : ParamSet();
}

QList<EventSubscription> TaskGroup::allEventSubscriptions() const {
  // LATER avoid creating the collection at every call
  return !isNull() ? data()->_onstart + data()->_onsuccess + data()->_onfailure
                   : QList<EventSubscription>();
}

QVariant TaskGroupData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
    case 11:
      return _id;
    case 1:
      return TaskGroup::parentGroupId(_id);
    case 2:
      if (role == Qt::EditRole)
        return _label == _id ? QVariant() : _label;
      return _label.isEmpty() ? _id : _label;
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

TaskGroupData *TaskGroup::data() {
  detach<TaskGroupData>();
  return (TaskGroupData*)SharedUiItem::data();
}

PfNode TaskGroup::toPfNode() const {
  const TaskGroupData *d = data();
  if (!d)
    return PfNode();
  PfNode node(QStringLiteral("taskgroup"), d->id());
  ConfigUtils::writeComments(&node, d->_commentsList);
  if (!d->_label.isNull())
    node.setAttribute(QStringLiteral("label"), d->_label);
  ConfigUtils::writeParamSet(&node, d->_params, QStringLiteral("param"));
  ConfigUtils::writeParamSet(&node, d->_setenv, QStringLiteral("setenv"));
  ConfigUtils::writeFlagSet(&node, d->_unsetenv, QStringLiteral("unsetenv"));
  ConfigUtils::writeEventSubscriptions(&node, d->_onstart);
  ConfigUtils::writeEventSubscriptions(&node, d->_onsuccess);
  ConfigUtils::writeEventSubscriptions(&node, d->_onfailure,
                                       excludeOnfinishSubscriptions);
  return node;
}

bool TaskGroup::setUiData(
    int section, const QVariant &value, QString *errorString,
    SharedUiItemDocumentTransaction *transaction, int role) {
  if (isNull())
    return false;
  return data()->setUiData(section, value, errorString, transaction, role);
}

bool TaskGroupData::setUiData(
    int section, const QVariant &value, QString *errorString,
    SharedUiItemDocumentTransaction *transaction, int role) {
  Q_ASSERT(transaction != 0);
  Q_ASSERT(errorString != 0);
  QString s = value.toString().trimmed();
  switch(section) {
  case 1: // changing parent group id is changing the begining of id itself
    if (_id.contains('.'))
      s = s+_id.mid(_id.lastIndexOf('.'));
    else
      s = s+"."+_id;
    // falling into next case
  case 0:
  case 11:
    s = ConfigUtils::sanitizeId(s, ConfigUtils::FullyQualifiedId);
    _id = s;
    return true;
  case 2:
    _label = value.toString().trimmed();
    if (_label == _id)
      _label = QString();
    return true;
  }
  return SharedUiItemData::setUiData(section, value, errorString, transaction,
                                     role);
}

Qt::ItemFlags TaskGroupData::uiFlags(int section) const {
  Qt::ItemFlags flags = SharedUiItemData::uiFlags(section);
  switch (section) {
  case 0:
  case 1:
  case 2:
  case 11:
    flags |= Qt::ItemIsEditable;
  }
  return flags;
}
