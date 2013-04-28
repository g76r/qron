/* Copyright 2012 Hallowyn and others.
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
#include "taskstreemodel.h"
#include <QtDebug>
#include <QDateTime>

#define COLUMNS 10

TasksTreeModel::TasksTreeModel(QObject *parent)
  : TreeModelWithStructure(parent) {
}

int TasksTreeModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant TasksTreeModel::data(const QModelIndex &index, int role) const {
  //qDebug() << "TasksTreeModel::data()" << index << index.isValid()
  //         << index.internalPointer();
  if (index.isValid()) {
    TreeItem *i = (TreeItem*)(index.internalPointer());
    //qDebug() << "  " << i;
    if (i) {
      //qDebug() << "  " << i << i->_isStructure << i->_id << i->_path;
      if (i->_isStructure) {
        TaskGroup g = _groups.value(i->_path);
        switch(role) {
        case Qt::DisplayRole:
          switch(index.column()) {
          case 0:
            //return QString("%1 %2 %3").arg((int)i).arg(g.id()).arg(i->_path);
            return i->_path;
          case 1:
            return g.label();
          case 8:
            return g.params().toString(false);
          }
          break;
        case TextViews::TrClassRole:
          return "";
        case TextViews::HtmlPrefixRole:
          if (index.column() == 0)
            return "<i class=\"icon-folder-open\"></i> ";
          break;
        default:
          ;
        }
      } else {
        // task
        Task t = _tasks.value(i->_path);
        switch(role) {
        case Qt::DisplayRole:
          switch(index.column()) {
          case 0:
            return i->_id;
          case 1:
            return t.label();
          case 2:
            return t.mean();
          case 3:
            return t.command();
          case 4:
            return t.target();
          case 5:
            return t.triggersAsString();
          case 6:
            return t.lastExecution();
          case 7:
            return t.nextScheduledExecution();
          case 8:
            return t.params().toString(false);
          case 9:
            return t.resourcesAsString();
          }
          break;
        case TextViews::HtmlPrefixRole:
          if (index.column() == 0)
            return "<i class=\"icon-cog\"></i> ";
          break;
        default:
          ;
        }
      }
    }
  }
  return QVariant();
}

QVariant TasksTreeModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Id";
    case 1:
      return "Label";
    case 2:
      return "Mean";
    case 3:
      return "Command";
    case 4:
      return "Target";
    case 5:
      return "Triggers";
    case 6:
      return "Last execution";
    case 7:
      return "Next execution";
    case 8:
      return "Parameters";
    case 9:
      return "Resources";
    }
  }
  return QVariant();
}

void TasksTreeModel::setAllTasksAndGroups(QHash<QString, TaskGroup> groups,
                                          QHash<QString, Task> tasks) {
  beginResetModel();
  QStringList names;
  foreach(QString id, groups.keys())
    names << id;
  names.sort(); // get a sorted groups id list
  foreach(QString id, names)
    getOrCreateItemByPath(id, true);
  names.clear();
  foreach(QString id, tasks.keys())
    names << id;
  names.sort(); // get a sorted tasks id list
  foreach(QString id, names)
    getOrCreateItemByPath(id, false);
  _groups = groups;
  _tasks = tasks;
  endResetModel();
  //qDebug() << "TasksTreeModel::setAllTasksAndGroups" << _root->_children.size()
  //         << _groups.size() << _tasks.size()
  //         << (_root->_children.size() ? _root->_children[0]->_id : "null");
}

void TasksTreeModel::taskChanged(Task task) {
  QModelIndex i = indexByPath(task.fqtn());
  if (i.isValid())
    emit dataChanged(i, i);
}

void TasksTreeModel::taskChanged(TaskRequest request) {
  taskChanged(request.task());
}
