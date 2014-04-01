/* Copyright 2013 Hallowyn and others.
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
#include "tasksmodel.h"
#include <QDateTime>
#include "config/eventsubscription.h"
#include <QUrl>
#include <QTimer>

// FIXME use SharedUiItemsTableModel

#define COLUMNS 32
// 60,000 ms = 1'
// should stay below HtmlTaskItemDelegate's SOON_EXECUTION_MILLIS
#define FULL_REFRESH_INTERVAL 60000

TasksModel::TasksModel(QObject *parent) : QAbstractTableModel(parent) {
  QTimer *timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(forceTimeRelatedDataRefresh()));
  timer->start(FULL_REFRESH_INTERVAL);
}

int TasksModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : _tasks.size();
}

int TasksModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant TasksModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _tasks.size()) {
    const Task t(_tasks.at(index.row()));
    switch(role) {
    case Qt::DisplayRole:
      switch(index.column()) {
      case 18:
        return _customActions.isEmpty()
            ? QVariant() : t.params().evaluate(_customActions, &t);
      }
      break;
    default:
      ;
    }
    return t.uiData(index.column(), role);
  }
  return QVariant();
}

QVariant TasksModel::headerData(int section, Qt::Orientation orientation,
                                int role) const {
  static Task t;
  return orientation == Qt::Horizontal ? t.uiHeaderData(section, role)
                                       : QVariant();
}

void TasksModel::configChanged(SchedulerConfig config) {
  beginResetModel();
  _tasks.clear();
  foreach (const Task task, config.tasks().values()) {
    int row;
    for (row = 0; row < _tasks.size(); ++row) {
      Task t2 = _tasks.at(row);
      // sort by taskgroupid then taskid
      if (task.taskGroup().id() < t2.taskGroup().id())
        break;
      if (task.taskGroup().id() == t2.taskGroup().id()
          && task.id() < t2.id())
        break;
    }
    _tasks.insert(row, task);
  }
  endResetModel();
}

void TasksModel::taskChanged(Task task) {
  for (int row = 0; row < _tasks.size(); ++row)
    if (_tasks.at(row).id() == task.id()) {
      emit dataChanged(createIndex(row, 0), createIndex(row, COLUMNS-1));
      return;
    }
}

void TasksModel::forceTimeRelatedDataRefresh() {
  int size = _tasks.size();
  if (size)
    emit dataChanged(createIndex(0, 10), createIndex(size-1, 10));
}
