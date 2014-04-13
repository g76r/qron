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
#ifndef TASK_P_H
#define TASK_P_H

/*
static int _uiTaskGroupColumnToTask[] =
{
  0, 2, 7, 14, 15,
  16, 20, 21, 22
};

static int _uiTaskColumnToTaskGroup[] =
{
  0, -1, 1, -1, -1,
  -1, 2, -1, -1, -1,
  0, -1, -1, 3, 4,
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
*/ // FIXME remove that stuff

static QString _uiHeaderNames[] = {
  "Short Id", // 0
  "Parent Group",
  "Label",
  "Mean",
  "Command",
  "Target", // 5
  "Trigger",
  "Parameters",
  "Resources",
  "Last execution",
  "Next execution", // 10
  "Id",
  "Max intances",
  "Instances count",
  "On start",
  "On success", // 15
  "On failure",
  "Instances / max",
  "Actions",
  "Last execution status",
  "System environment", // 20
  "Setenv",
  "Unsetenv",
  "Min expected duration",
  "Max expected duration",
  "Request-time overridable params", // 25
  "Last execution duration",
  "Max duration before abort",
  "Triggers with calendars",
  "Enabled",
  "Has triggers with calendars", // 30
  "Parent task"
};

#endif // TASK_P_H
