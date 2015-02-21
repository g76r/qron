/* Copyright 2014-2015 Hallowyn and others.
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
#include "graphvizdiagramsbuilder.h"
#include "graphviz_styles.h"
#include "action/action.h"
#include "trigger/crontrigger.h"
#include "trigger/noticetrigger.h"
#include "config/eventsubscription.h"
#include "config/step.h"
#include "sched/stepinstance.h"

GraphvizDiagramsBuilder::GraphvizDiagramsBuilder() {
}

QHash<QString,QString> GraphvizDiagramsBuilder
::configDiagrams(SchedulerConfig config) {
  QHash<QString,Task> tasks = config.tasks();
  QHash<QString,Cluster> clusters = config.clusters();
  QHash<QString,Host> hosts = config.hosts();
  QList<EventSubscription> schedulerEventsSubscriptions
      = config.allEventsSubscriptions();
  QSet<QString> displayedGroups, displayedHosts, notices, taskIds,
      displayedGlobalEventsName;
  QHash<QString,QString> diagrams;
  // search for:
  // * displayed groups, i.e. (i) actual groups containing at less one task
  //   and (ii) virtual parent groups (e.g. a group "foo.bar" as a virtual
  //   parent "foo" which is not an actual group but which make the rendering
  //   more readable by making  visible the dot-hierarchy tree)
  // * displayed hosts, i.e. hosts that are targets of at less one cluster
  //   or one task
  // * notices, deduced from notice triggers and postnotice events (since
  //   notices are not explicitely declared in configuration objects)
  // * task ids, which are usefull to detect inexisting tasks and avoid drawing
  //   edges to or from them
  // * displayed global event names, i.e. global event names (e.g. onstartup)
  //   with at less one displayed event subscription (e.g. requesttask,
  //   postnotice, emitalert)
  // * (workflow) subtasks tree
  foreach (const Task &task, tasks.values()) {
    QString s = task.taskGroup().id();
    displayedGroups.insert(s);
    for (int i = 0; (i = s.indexOf('.', i+1)) > 0; )
      displayedGroups.insert(s.mid(0, i));
    s = task.target();
    if (!s.isEmpty())
      displayedHosts.insert(s);
    taskIds.insert(task.id());
  }
  foreach (const Cluster &cluster, clusters.values())
    foreach (const Host &host, cluster.hosts())
      if (!host.isNull())
        displayedHosts.insert(host.id());
  foreach (const Task &task, tasks.values()) {
    foreach (const NoticeTrigger &trigger, task.noticeTriggers())
      notices.insert(trigger.expression());
    foreach (const QString &notice,
             task.workflowTriggerSubscriptionsByNotice().keys())
      notices.insert(notice);
    foreach (const EventSubscription &sub,
             task.allEventsSubscriptions()
             + task.taskGroup().allEventSubscriptions())
      foreach (const Action &action, sub.actions()) {
        if (action.actionType() == "postnotice")
          notices.insert(action.targetName());
      }
  }
  foreach (const EventSubscription &sub, schedulerEventsSubscriptions) {
    foreach (const Action &action, sub.actions()) {
      QString actionType = action.actionType();
      if (actionType == "postnotice")
        notices.insert(action.targetName());
      if (actionType == "postnotice" || actionType == "requesttask")
        displayedGlobalEventsName.insert(sub.eventName());
    }
  }
  QMultiHash<QString,Task> subtasks;
  foreach (const Task &task, tasks.values()) {
    foreach (const Task &subtask, tasks.values()) {
      if (!subtask.supertaskId().isNull()
          && subtask.supertaskId() == task.id())
        subtasks.insert(task.id(), subtask);
    }
  }
  /***************************************************************************/
  // tasks deployment diagram
  QString gv;
  gv.append("graph \"tasks deployment diagram\" {\n"
            "graph[" GLOBAL_GRAPH "]\n"
            "subgraph{graph[rank=max]\n");
  foreach (const Host &host, hosts.values())
    if (displayedHosts.contains(host.id()))
      gv.append("\"").append(host.id()).append("\"").append("[label=\"")
          .append(host.id()).append(" (")
          .append(host.hostname()).append(")\"," HOST_NODE "]\n");
  gv.append("}\n");
  foreach (const Cluster &cluster, clusters.values())
    if (!cluster.isNull()) {
      gv.append("\"").append(cluster.id()).append("\"")
          .append("[label=\"").append(cluster.id()).append("\\n(")
          .append(cluster.balancingAsString())
          .append(")\"," CLUSTER_NODE "]\n");
      foreach (const Host &host, cluster.hosts())
        gv.append("\"").append(cluster.id()).append("\"--\"").append(host.id())
            .append("\"[" CLUSTER_HOST_EDGE "]\n");
    }
  gv.append("subgraph{graph[rank=min]\n");
  foreach (const QString &id, displayedGroups) {
    if (!id.contains('.')) // root groups
      gv.append("\"").append(id).append("\" [" TASKGROUP_NODE "]\n");
  }
  gv.append("}\n");
  foreach (const QString &id, displayedGroups) {
    if (id.contains('.')) // non root groups
      gv.append("\"").append(id).append("\" [" TASKGROUP_NODE "]\n");
  }
  foreach (const QString &parent, displayedGroups) {
    QString prefix = parent + ".";
    foreach (const QString &child, displayedGroups) {
      if (child.startsWith(prefix))
        gv.append("\"").append(parent).append("\" -- \"")
            .append(child).append("\" [" TASKGROUP_EDGE "]\n");
    }
  }
  foreach (const Task &task, tasks.values()) {
    // ignore subtasks
    if (!task.supertaskId().isNull())
      continue;
    // draw task node and group--task edge
    gv.append("\""+task.id()+"\" [label=\""+task.localId()+"\","
              +(task.mean() == Task::Workflow ? WORKFLOW_TASK_NODE : TASK_NODE)
              +",tooltip=\""+task.id()+"\"]\n");
    gv.append("\"").append(task.taskGroup().id()).append("\"--")
        .append("\"").append(task.id())
        .append("\" [" TASKGROUP_TASK_EDGE "]\n");
    // draw task--target edges, workflow tasks showing all edge applicable to
    // their subtasks once and only once
    QSet<QString> edges;
    QList<Task> deployed = subtasks.values(task.id());
    if (deployed.isEmpty())
      deployed.append(task);
    foreach (const Task &subtask, deployed)
      edges.insert("\""+task.id()+"\"--\""+subtask.target()+"\" [label=\""
                   +Task::meanAsString(subtask.mean())
                   +"\"," TASK_TARGET_EDGE "]\n");
    foreach (QString s, edges)
      gv.append(s);
  }
  gv.append("}");
  diagrams.insert("tasksDeploymentDiagram", gv);
  /***************************************************************************/
  // tasks trigger diagram
  gv.clear();
  gv.append("graph \"tasks trigger diagram\" {\n"
            "graph[" GLOBAL_GRAPH "]\n"
            "subgraph{graph[rank=max]\n");
  foreach (const QString &cause, displayedGlobalEventsName)
    gv.append("\"$global_").append(cause).append("\" [label=\"")
        .append(cause).append("\"," GLOBAL_EVENT_NODE "]\n");
  gv.append("}\n");
  foreach (QString notice, notices) {
    notice.remove('"');
    gv.append("\"$notice_").append(notice).append("\"")
        .append("[label=\"^").append(notice).append("\"," NOTICE_NODE "]\n");
  }
  // root groups
  gv.append("subgraph{graph[rank=min]\n");
  foreach (const QString &id, displayedGroups) {
    if (!id.contains('.')) // root groups
      gv.append("\"").append(id).append("\" [" TASKGROUP_NODE "]\n");
  }
  gv.append("}\n");
  // other groups
  foreach (const QString &id, displayedGroups) {
    if (id.contains('.')) // non root groups
      gv.append("\"").append(id).append("\" [" TASKGROUP_NODE "]\n");
  }
  // groups edges
  foreach (const QString &parent, displayedGroups) {
    QString prefix = parent + ".";
    foreach (const QString &child, displayedGroups) {
      if (child.startsWith(prefix))
        gv.append("\"").append(parent).append("\" -- \"")
            .append(child).append("\" [" TASKGROUP_EDGE "]\n");
    }
  }
  // tasks
  int cronid = 0;
  foreach (const Task &task, tasks.values()) {
    // ignore subtasks
    if (!task.supertaskId().isNull())
      continue;
    // task nodes and group--task edges
    gv.append("\""+task.id()+"\" [label=\""+task.localId()+"\","
              +(task.mean() == Task::Workflow ? WORKFLOW_TASK_NODE : TASK_NODE)
              +",tooltip=\""+task.id()+"\"]\n");
    gv.append("\"").append(task.taskGroup().id()).append("\"--")
        .append("\"").append(task.id())
        .append("\" [" TASKGROUP_TASK_EDGE "]\n");
    // cron triggers
    foreach (const CronTrigger &cron, task.cronTriggers()) {
      gv.append("\"$cron_").append(QString::number(++cronid))
          .append("\" [label=\"(").append(cron.expression())
          .append(")\"," CRON_TRIGGER_NODE "]\n");
      gv.append("\"").append(task.id()).append("\"--\"$cron_")
          .append(QString::number(cronid))
          .append("\" [" TASK_TRIGGER_EDGE "]\n");
    }
    // workflow internal cron triggers
    foreach (const CronTrigger &cron,
             task.workflowCronTriggersById().values()) {
      gv.append("\"$cron_").append(QString::number(++cronid))
          .append("\" [label=\"(").append(cron.expression())
          .append(")\"," CRON_TRIGGER_NODE "]\n");
      gv.append("\"").append(task.id()).append("\"--\"$cron_")
          .append(QString::number(cronid))
          .append("\" [" WORKFLOW_TASK_TRIGGER_EDGE "]\n");

    }
    // notice triggers
    foreach (const NoticeTrigger &trigger, task.noticeTriggers())
      gv.append("\"").append(task.id()).append("\"--\"$notice_")
          .append(trigger.expression().remove('"'))
          .append("\" [" TASK_TRIGGER_EDGE "]\n");
    // workflow internal notice triggers
    foreach (QString notice,
             task.workflowTriggerSubscriptionsByNotice().keys()) {
      gv.append("\"").append(task.id()).append("\"--\"$notice_")
          .append(notice.remove('"'))
          .append("\" [" WORKFLOW_TASK_TRIGGER_EDGE "]\n");
    }
    // no trigger pseudo-trigger
    if (task.noticeTriggers().isEmpty() && task.cronTriggers().isEmpty()
        && task.otherTriggers().isEmpty()) {
      gv.append("\"$notrigger_").append(QString::number(++cronid))
          .append("\" [label=\"no trigger\"," NO_TRIGGER_NODE "]\n");
      gv.append("\"").append(task.id()).append("\"--\"$notrigger_")
          .append(QString::number(cronid))
          .append("\" [" TASK_NOTRIGGER_EDGE "]\n");
    }
    // events defined at task level
    QSet<QString> edges;
    foreach (const EventSubscription &sub,
             task.allEventsSubscriptions() + task.taskGroup().allEventSubscriptions()) {
      foreach (const Action &action, sub.actions()) {
        QString actionType = action.actionType();
        if (actionType == "postnotice") {
          gv.append("\"").append(task.id()).append("\"--\"$notice_")
              .append(action.targetName().remove('"')).append("\" [label=\"")
              .append(sub.humanReadableCause())
              .append("\"," TASK_POSTNOTICE_EDGE "]\n");
        } else if (actionType == "requesttask") {
          QString target = action.targetName();
          if (!target.contains('.'))
            target = task.taskGroup().id()+"."+target;
          if (taskIds.contains(target))
            edges.insert("\""+task.id()+"\"--\""+target+"\" [label=\""
                         +sub.humanReadableCause()
                         +"\"," TASK_REQUESTTASK_EDGE "]\n");
        }
      }
    }
    foreach (const QString &edge, edges)
      gv.append(edge);
  }
  // events defined globally
  foreach (const EventSubscription &sub, schedulerEventsSubscriptions) {
    foreach (const Action &action, sub.actions()) {
      QString actionType = action.actionType();
      if (actionType == "postnotice") {
        gv.append("\"$notice_").append(action.targetName().remove('"'))
            .append("\"--\"$global_").append(sub.eventName())
            .append("\" [" GLOBAL_POSTNOTICE_EDGE ",label=\"")
            .append(sub.humanReadableCause().remove('"')).append("\"]\n");
      } else if (actionType == "requesttask") {
        QString target = action.targetName();
        if (taskIds.contains(target)) {
          gv.append("\"").append(target).append("\"--\"$global_")
              .append(sub.eventName())
              .append("\" [" GLOBAL_REQUESTTASK_EDGE ",label=\"")
              .append(sub.humanReadableCause().remove('"')).append("\"]\n");
        }
      }
    }
  }
  gv.append("}");
  diagrams.insert("tasksTriggerDiagram", gv);
  return diagrams;
  // LATER add a full events diagram with log, udp, etc. events
}

QString GraphvizDiagramsBuilder::workflowTaskDiagram(Task task) {
  return workflowTaskDiagram(task, QHash<QString,StepInstance>());
}

QString GraphvizDiagramsBuilder::workflowTaskDiagram(
    Task task, QHash<QString,StepInstance> stepInstances) {
  // LATER implement instanciated workflow diagram for real
  if (task.mean() != Task::Workflow)
    return QString();
  QString gv("graph \""+task.id()+" workflow\"{\n"
             "  graph[" WORKFLOW_GRAPH " ]\n"
             "  \"step_$start\"[" START_NODE "]\n"
             "  \"step_$end\"[" END_NODE "]\n");
  // build nodes from steps (apart from $start and $end)
  foreach (Step s, task.steps().values()) {
    StepInstance si = stepInstances.value(s.id());
    switch (s.kind()) {
    case Step::SubTask:
      gv.append("  \"step_"+s.localId()+"\"[label=\""+s.localId()+"\","
                TASK_NODE+(si.isReady() ? ",color=orange" : "")
                +",tooltip=\""+s.subtask().id()+"\"]\n");
      // LATER red if failure, green if ok
      break;
    case Step::AndJoin:
      gv.append("  \"step_"+s.localId()+"\"[" ANDJOIN_NODE
                +(si.isReady() ? ",color=greenforest" : "")+"]\n");
      break;
    case Step::OrJoin:
      gv.append("  \"step_"+s.localId()+"\"[" ORJOIN_NODE
                +(si.isReady() ? ",color=greenforest" : "")+"]\n");
      break;
    case Step::Start:
    case Step::End:
    case Step::Unknown:
      ;
    }
  }
  QSet<QString> gvedges; // use a QSet to remove duplicate onfinish edges
  // build edges from workflow triggers
  foreach (QString tsId, task.workflowTriggerSubscriptionsById().keys()) {
    const WorkflowTriggerSubscription &ts
        = task.workflowTriggerSubscriptionsById().value(tsId);
    QString expr = ts.trigger().humanReadableExpression().remove('"');
    // LATER do not rely on human readable expr to determine trigger style
    gv.append("  \"trigger_"+tsId+"\"[label=\""+expr+"\","
              +(expr.startsWith('^')?NOTICE_NODE:CRON_TRIGGER_NODE)+"]\n");
    foreach (Action a, ts.eventSubscription().actions()) {
      if (a.actionType() == "step") {
        QString target = a.targetName();
        if (task.steps().contains(task.id()+":"+target)) {
          gvedges.insert("  \"trigger_"+tsId+"\" -- \"step_"+target
                         +"\"[label=\"\"," TRIGGER_STEP_EDGE  "]\n");
        } else {
          // do nothing, the warning is issued in another loop
        }
      } else if (a.actionType() == "end") {
        gvedges.insert("  \"trigger_"+tsId+"\" -- \"step_$end\" [label=\"\","
                TRIGGER_STEP_EDGE "]\n");
      }
    }
  }
  // build edges from step event subscriptions
  foreach (Step source, task.steps()) {
    QList<EventSubscription> subList
        = source.onreadyEventSubscriptions()
        + source.subtask().allEventsSubscriptions()
        + source.subtask().taskGroup().allEventSubscriptions();
    QString sourceLocalId = source.localId();
    foreach (EventSubscription es, subList) {
      foreach (Action a, es.actions()) {
        if (a.actionType() == "step") {
          QString target = a.targetName();
          if (task.steps().contains(task.id()+":"+target)) {
            gvedges.insert("  \"step_"+sourceLocalId+"\" -- \"step_"+target
                           +"\"[label=\""+es.eventName()+"\"," STEP_EDGE "]\n");
          }
        } else if (a.actionType() == "end") {
          gvedges.insert("  \"step_"+sourceLocalId
                         +"\" -- \"step_$end\" [label=\""+es.eventName()+"\","
                         STEP_EDGE "]\n");
        }
      }
    }
  }
  foreach(QString s, gvedges)
    gv.append(s);
  gv.append("}");
  return gv;
}
