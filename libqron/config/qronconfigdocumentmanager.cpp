/* Copyright 2015 Hallowyn and others.
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
  registerItemType(
        "taskgroup", &TaskGroup::setUiData, [this](QString id) -> SharedUiItem {
    return TaskGroup(PfNode("taskgroup", id), _config.globalParams(),
                     _config.setenv(), _config.unsetenv(), 0);
  });
  addChangeItemTrigger(
        "taskgroup", AfterUpdate,
        [](SharedUiItemDocumentTransaction *transaction,
        SharedUiItem *newItem, SharedUiItem oldItem,
        QString idQualifier, QString *errorString) {
    Q_UNUSED(oldItem)
    Q_UNUSED(idQualifier)
    TaskGroup *newGroup = static_cast<TaskGroup*>(newItem);
    if (newItem->id() != oldItem.id()) {
      SharedUiItemList<> taskItems =
          transaction->foreignKeySources("task", 1, oldItem.id());
      foreach (SharedUiItem oldTaskItem, taskItems) {
        Task oldTask = static_cast<Task&>(oldTaskItem);
        if (oldTask.workflowTaskId().isEmpty()) {
          Task newTask = oldTask;
          newTask.setTaskGroup(*newGroup);
          // update standalone and workflow first, workflows task will update subtasks
          if (!transaction->changeItem(newTask, oldTask, "task", errorString))
            return false;
        }
      }
    }
    return true;
  });
  registerItemType(
        "task", &Task::setUiData,
        [this](SharedUiItemDocumentTransaction *transaction, QString id,
        QString *errorString) -> SharedUiItem {
    Q_UNUSED(transaction)
    Q_UNUSED(id)
    *errorString = "Cannot create task outside GUI";
    return SharedUiItem();
  });
  addForeignKey("task", 1, "taskgroup", 0, NoAction, Cascade);
  addForeignKey("task", 31, "task", 11, NoAction, Cascade); // workflowtaskid
  addChangeItemTrigger(
        "task", AfterUpdate|AfterDelete,
        [](SharedUiItemDocumentTransaction *transaction, SharedUiItem *newItem,
        SharedUiItem oldItem, QString idQualifier, QString *errorString) {
    Q_UNUSED(oldItem)
    Q_UNUSED(idQualifier)
    qDebug() << "trigger1" << newItem->id();
    Task *newTask = static_cast<Task*>(newItem);
    Task &oldTask = static_cast<Task&>(oldItem);
    if (newTask->mean() != Task::Workflow && oldTask.mean() == Task::Workflow) {
      foreach (SharedUiItem oldTask,
               transaction->foreignKeySources("task", 31, oldItem.id())) {
        // when a task is no longer a workflow, delete all its subtasks (tasks which have it as their workflowtaskid)
        if (!transaction->changeItem(
              SharedUiItem(), oldTask, "task", errorString))
          return false;
      }
    }
    if (newTask->mean() == Task::Workflow) { // implicitely: !newTask->isNull()
      foreach (SharedUiItem oldSubTask,
               transaction->foreignKeySources("task", 31, oldItem.id())) {
        Task newSubTask = static_cast<Task&>(oldSubTask);
        newSubTask.setWorkflowTask(*newTask);
        // when a task is a workflow, update its subtasks
        if (!transaction->changeItem(newSubTask, oldSubTask, "task",
                                     errorString))
          return false;
      }
    }
    return true;
  });
  registerItemType(
        "host", &Host::setUiData,
        [](QString id) -> SharedUiItem { return Host(PfNode("host", id)); });
  addChangeItemTrigger(
        "host", BeforeUpdate|BeforeCreate,
        [](SharedUiItemDocumentTransaction *transaction, SharedUiItem *newItem,
        SharedUiItem oldItem, QString idQualifier, QString *errorString) {
    Q_UNUSED(oldItem)
    Q_UNUSED(idQualifier)
    if (!transaction->itemById("cluster", newItem->id()).isNull()) {
      *errorString = "Host id already used by a cluster: "+newItem->id();
      return false;
    }
    return true;
  });
  addChangeItemTrigger(
        "host", AfterUpdate|AfterDelete,
        [](SharedUiItemDocumentTransaction *transaction, SharedUiItem *newItem,
        SharedUiItem oldItem, QString idQualifier, QString *errorString) {
    Q_UNUSED(idQualifier)
    // cannot be a fk because target can reference either a host or a cluster
    foreach (const SharedUiItem &oldTaskItem,
             transaction->foreignKeySources("task", 5, oldItem.id())) {
      const Task &oldTask = static_cast<const Task&>(oldTaskItem);
      Task newTask = oldTask;
      newTask.setTarget(newItem->id());
      if (!transaction->changeItem(newTask, oldTask, "task", errorString))
        return false;
    }
    // on host change, upgrade every cluster it belongs to
    foreach (const SharedUiItem &oldClusterItem,
             transaction->itemsByIdQualifier("cluster")) {
      auto &oldCluster = static_cast<const Cluster &>(oldClusterItem);
      SharedUiItemList<Host> hosts = oldCluster.hosts();
      for (int i = 0; i < hosts.size(); ++i) {
        if(hosts[i] == oldItem) {
          Cluster newCluster = oldCluster;
          if (newItem->isNull())
            hosts.removeAt(i);
          else
            hosts[i] = static_cast<Host&>(*newItem);
          newCluster.setHosts(hosts);
          if (!transaction->changeItem(newCluster, oldCluster, "cluster",
                                       errorString))
            return false;
          break;
        }
      }
    }
    return true;
  });
  registerItemType(
        "cluster", &Cluster::setUiData, [](QString id) -> SharedUiItem {
    return Cluster(PfNode("cluster", id));
  });
  addChangeItemTrigger(
        "cluster", BeforeUpdate|BeforeCreate,
        [](SharedUiItemDocumentTransaction *transaction, SharedUiItem *newItem,
        SharedUiItem oldItem, QString idQualifier, QString *errorString) {
    Q_UNUSED(oldItem)
    Q_UNUSED(idQualifier)
    if (!transaction->itemById("host", newItem->id()).isNull()) {
      *errorString = "Cluster id already used by a host: "+newItem->id();
      return false;
    }
    return true;
  });
  addChangeItemTrigger(
        "cluster", AfterUpdate|AfterDelete,
        [](SharedUiItemDocumentTransaction *transaction, SharedUiItem *newItem,
        SharedUiItem oldItem, QString idQualifier, QString *errorString) {
    Q_UNUSED(oldItem)
    Q_UNUSED(idQualifier)
    // cannot be a fk because target can reference either a host or a cluster
    foreach (const SharedUiItem &oldTaskItem,
             transaction->foreignKeySources("task", 5, oldItem.id())) {
      const Task &oldTask = static_cast<const Task&>(oldTaskItem);
      Task newTask = oldTask;
      newTask.setTarget(newItem->id());
      if (!transaction->changeItem(newTask, oldTask, "task", errorString))
        return false;
    }
    return true;
  });
  // TODO register other items kinds
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
    SharedUiItemList<Step> steps;
    foreach (const Task &task, _config.tasks())
      steps.append(task.steps().values());
    return steps;
  } else if (idQualifier == "workflowtransition") {
    // TODO
  }
  return SharedUiItemList<SharedUiItem>();
}

static SharedUiItem nullItem;

template<class T>
void inline QronConfigDocumentManager::emitSignalForItemTypeChanges(
    QHash<QString,T> newItems, QHash<QString,T> oldItems, QString idQualifier) {
  foreach (const T &oldItem, oldItems)
    if (!newItems.contains(oldItem.id()))
      emit itemChanged(nullItem, oldItem, idQualifier);
  foreach (const T &newItem, newItems)
    emit itemChanged(newItem, oldItems.value(newItem.id()), idQualifier);
}

template<>
void inline QronConfigDocumentManager::emitSignalForItemTypeChanges<Task>(
    QHash<QString,Task> newItems, QHash<QString,Task> oldItems,
    QString idQualifier) {
  foreach (const Task &oldItem, oldItems) {
    if (!newItems.contains(oldItem.id())) {
      emit itemChanged(nullItem, oldItem, idQualifier);
    }
    QHash<QString,Step> newSteps = newItems.value(oldItem.id()).steps();
    foreach(const Step &oldStep, oldItem.steps()) {
      if (!newSteps.contains(oldStep.id()))
        emit itemChanged(nullItem, oldStep, QStringLiteral("step"));
    }
  }
  QList<Task> newList = newItems.values();
  // sorting tasks by id ensure that workflows are changed before their subtasks
  qSort(newList);
  foreach (const Task &newItem, newList) {
    const Task &oldItem = oldItems.value(newItem.id());
    emit itemChanged(newItem, oldItem, idQualifier);
    QHash<QString,Step> oldSteps = oldItem.steps();
    foreach(const Step &newStep, newItem.steps()) {
      emit itemChanged(newStep, oldSteps.value(newStep.id()),
                       QStringLiteral("step"));
    }
  }
}

void QronConfigDocumentManager::setConfig(SchedulerConfig newConfig) {
  SchedulerConfig oldConfig = _config;
  _config = newConfig;
  emit globalParamsChanged(newConfig.globalParams(), oldConfig.globalParams(),
                           QStringLiteral("globalparams"));
  emit globalSetenvsChanged(newConfig.setenv(), oldConfig.setenv(),
                           QStringLiteral("globalsetenvs"));
  emit globalUnsetenvsChanged(newConfig.unsetenv(), oldConfig.unsetenv(),
                             QStringLiteral("globalunsetenvs"));
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
  emitSignalForItemTypeChanges(
        newConfig.tasks(), oldConfig.tasks(), QStringLiteral("task"));
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
