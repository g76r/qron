/* Copyright 2014 Hallowyn and others.
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
#ifndef GRAPHVIZ_STYLES_H
#define GRAPHVIZ_STYLES_H

#define GLOBAL_GRAPH "rankdir=LR,bgcolor=transparent,splines=ortho"
#define CLUSTER_NODE "shape=box,peripheries=2,style=filled,fillcolor=coral"
#define HOST_NODE "shape=box,style=filled,fillcolor=coral"
#define TASK_NODE "shape=box,style=\"rounded,filled\",fillcolor=skyblue"
#define WORKFLOW_TASK_NODE TASK_NODE ",peripheries=2"
#define TASKGROUP_NODE "shape=ellipse,style=dashed"
#define CLUSTER_HOST_EDGE "dir=forward,arrowhead=vee"
#define TASK_TARGET_EDGE "dir=forward,arrowhead=vee"
#define TASKGROUP_TASK_EDGE "style=dashed"
#define TASKGROUP_EDGE "style=dashed"
#define NOTICE_NODE "shape=note,style=filled,fillcolor=wheat"
#define CRON_TRIGGER_NODE "shape=none"
#define NO_TRIGGER_NODE "shape=none"
#define TASK_TRIGGER_EDGE "dir=back,arrowtail=vee"
#define TASK_NOTRIGGER_EDGE "dir=back,arrowtail=odot,style=dashed"
#define TASK_POSTNOTICE_EDGE "dir=forward,arrowhead=vee"
#define TASK_REQUESTTASK_EDGE TASK_POSTNOTICE_EDGE
#define GLOBAL_EVENT_NODE "shape=none"
//#define GLOBAL_EVENT_NODE "shape=pentagon"
#define GLOBAL_POSTNOTICE_EDGE "dir=back,arrowtail=vee"
#define GLOBAL_REQUESTTASK_EDGE TASK_POSTNOTICE_EDGE
#define WORKFLOW_GRAPH "bgcolor=transparent"
#define ANDJOIN_NODE "shape=square,label=and"
#define ORJOIN_NODE "shape=circle,label=or"
#define START_NODE "shape=circle,style=\"filled\",width=.2,label=\"\",fillcolor=black"
#define END_NODE "shape=doublecircle,style=\"filled\",width=.2,label=\"\",fillcolor=black"
#define STEP_EDGE "dir=forward,arrowhead=vee"
#define TRIGGER_STEP_EDGE "dir=forward,arrowhead=vee,style=dashed"
#define WORKFLOW_TASK_TRIGGER_EDGE "dir=back,arrowtail=dot,style=dotted"

#endif // GRAPHVIZ_STYLES_H
