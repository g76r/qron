/* Copyright 2015 Hallowyn and others.
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
#include "qronconfigdocumentmanager.h"
#include <QtDebug>
#include "task.h"
#include "step.h"

static int staticInit() {
  qRegisterMetaType<Host>("Host");
  qRegisterMetaType<Host>("Cluster");
  qRegisterMetaType<Calendar>("Calendar");
  qRegisterMetaType<Task>("Task");
  qRegisterMetaType<Task>("TaskGroup");
  qRegisterMetaType<WorkflowTransition>("WorkflowTransition");
  qRegisterMetaType<LogFile>("LogFile");
  qRegisterMetaType<QList<EventSubscription>>("QList<EventSubscription>");
  qRegisterMetaType<QList<LogFile>>("QList<LogFile>");
  qRegisterMetaType<SchedulerConfig>("SchedulerConfig");
  qRegisterMetaType<AccessControlConfig>("AccessControlConfig");
  qRegisterMetaType<AlerterConfig>("AlerterConfig");
  // TODO is the following really/still needed ?
  qRegisterMetaType<QHash<QString,Task>>("QHash<QString,Task>");
  qRegisterMetaType<QHash<QString,TaskGroup>>("QHash<QString,TaskGroup>");
  qRegisterMetaType<QHash<QString,Cluster>>("QHash<QString,Cluster>");
  qRegisterMetaType<QHash<QString,Host>>("QHash<QString,Host>");
  qRegisterMetaType<QHash<QString,Calendar>>("QHash<QString,Calendar>");
  qRegisterMetaType<QHash<QString,qint64> >("QHash<QString,qint64>");
  return 0;
}
Q_CONSTRUCTOR_FUNCTION(staticInit)

QronConfigDocumentManager::QronConfigDocumentManager(QObject *parent)
  : SharedUiItemDocumentManager(parent) {
}

SharedUiItem QronConfigDocumentManager::itemById(
    QString idQualifier, QString id) const {
  // TODO also implement for other items
  if (idQualifier == "task") {
    return _config.tasks().value(id);
  } else if (idQualifier == "taskgroup") {
    return _config.tasksGroups().value(id);
  } else if (idQualifier == "host") {
    return _config.hosts().value(id);
  } else if (idQualifier == "cluster") {
    return _config.clusters().value(id);
  } else if (idQualifier == "step") {
    QString workflowId = id.mid(id.indexOf(':')+1);
    return _config.tasks().value(workflowId).steps().value(id);
  } else if (idQualifier == "workflowtransition") {
    QString workflowId = id.mid(id.indexOf(':')+1);
    QList<WorkflowTransition> transitions = _config.tasks().value(workflowId)
        .workflowTransitionsBySourceLocalId().values();
    foreach (WorkflowTransition transition, transitions)
      if (transition.id() == id)
        return transition;
  }
  return SharedUiItem();
}

SharedUiItemList<SharedUiItem> QronConfigDocumentManager
::itemsByIdQualifier(QString idQualifier) const {
  // TODO also implement for other items
  if (idQualifier == "task") {
    return SharedUiItemList<Task>(_config.tasks().values());
  } else if (idQualifier == "taskgroup") {
    return SharedUiItemList<TaskGroup>(_config.tasksGroups().values());
  } else if (idQualifier == "host") {
    return SharedUiItemList<Host>(_config.hosts().values());
  } else if (idQualifier == "cluster") {
    return SharedUiItemList<Cluster>(_config.clusters().values());
  } else if (idQualifier == "calendar") {
    // LATER is it right to return only *named* calendars ?
    return SharedUiItemList<Calendar>(_config.namedCalendars().values());
  } else if (idQualifier == "step") {
    // TODO
  } else if (idQualifier == "workflowtransition") {
    // TODO
  }
  return SharedUiItemList<SharedUiItem>();
}

static SharedUiItem nullItem;

template<class T>
void inline QronConfigDocumentManager::emitSignalForItemTypeChanges(
    QHash<QString,T> newItems, QHash<QString,T> oldItems, QString idQualifier,
    bool sortNewItems) {
  foreach (const T &oldItem, oldItems)
    if (!newItems.contains(oldItem.id()))
      emit itemChanged(nullItem, oldItem, idQualifier);
  if (sortNewItems) {
    QList<T> newList = newItems.values();
    qSort(newList);
    foreach (const T &newItem, newList)
      emit itemChanged(newItem, oldItems.value(newItem.id()), idQualifier);
  } else {
    foreach (const T &newItem, newItems)
      emit itemChanged(newItem, oldItems.value(newItem.id()), idQualifier);
  }
}

void QronConfigDocumentManager::setConfig(SchedulerConfig newConfig) {
  SchedulerConfig oldConfig = _config;
  _config = newConfig;
  emit globalParamsChanged(newConfig.globalParams());
  emit globalSetenvChanged(newConfig.setenv());
  emit globalUnsetenvChanged(newConfig.unsetenv());
  emit accessControlConfigurationChanged(
        !newConfig.accessControlConfig().isEmpty());
  emitSignalForItemTypeChanges(
        newConfig.hosts(), oldConfig.hosts(), QStringLiteral("host"));
  emitSignalForItemTypeChanges(
        newConfig.clusters(), oldConfig.clusters(), QStringLiteral("cluster"));
  emitSignalForItemTypeChanges(
        newConfig.namedCalendars(), oldConfig.namedCalendars(),
        QStringLiteral("calendar"));
  emitSignalForItemTypeChanges(
        newConfig.tasksGroups(), oldConfig.tasksGroups(),
        QStringLiteral("taskgroup"));
  // sorting tasks by id ensure that workflows are changed before their subtasks
  emitSignalForItemTypeChanges(
        newConfig.tasks(), oldConfig.tasks(), QStringLiteral("task"), true);
  // TODO also implement for other items
  emit globalEventSubscriptionsChanged(
        newConfig.onstart(), newConfig.onsuccess(), newConfig.onfailure(),
        newConfig.onlog(), newConfig.onnotice(), newConfig.onschedulerstart(),
        newConfig.onconfigload());
}

bool QronConfigDocumentManager::prepareChangeItem(
    SharedUiItemDocumentTransaction *transaction, SharedUiItem newItem,
    SharedUiItem oldItem, QString idQualifier, QString *errorString) {
  QString oldId = oldItem.id(), newId = newItem.id(), reason;
  if (idQualifier == "taskgroup") {
    // currently nothing to do
  } else if (idQualifier == "task") {
    // currently nothing to do
  } else if (idQualifier == "cluster") {
    // currently nothing to do
  } else if (idQualifier == "host") {
    // currently nothing to do
  } else if (idQualifier == "calendar") {
    // currently nothing to do
  } else {
    reason = "QronConfigDocumentManager::changeItem do not support item type: "
        +idQualifier;
  }
  // TODO implement more item types
  if (reason.isNull()) {
    storeItemChange(transaction, newItem, oldItem, idQualifier);
    // FIXME mute
    qDebug() << "QronConfigDocumentManager::prepareChangeItem succeeded:"
             << idQualifier << newItem.id() << oldId;
    return true;
  } else {
    if (errorString)
      *errorString = reason;
    return false;
  }
}

void QronConfigDocumentManager::commitChangeItem(
    SharedUiItem newItem, SharedUiItem oldItem, QString idQualifier) {
  _config.changeItem(newItem, oldItem, idQualifier);
  // FIXME mute
  qDebug() << "QronConfigDocumentManager::commitChangeItem done"
           << newItem.qualifiedId() << oldItem.qualifiedId();
           //<< _config.toPfNode().toString();
  SharedUiItemDocumentManager::commitChangeItem(newItem, oldItem, idQualifier);
}
