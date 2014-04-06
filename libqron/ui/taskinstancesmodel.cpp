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
#include "taskinstancesmodel.h"

#define COLUMNS 10

TaskInstancesModel::TaskInstancesModel(QObject *parent, int maxrows,
                                     bool keepFinished)
  : QAbstractTableModel(parent), _maxrows(maxrows), _keepFinished(keepFinished) {
}

int TaskInstancesModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _instances.size();
}

int TaskInstancesModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant TaskInstancesModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _instances.size()) {
    const TaskInstance r(_instances.at(index.row()));
    switch(role) {
    case Qt::DisplayRole: {
      switch(index.column()) {
      case 0:
        return r.id();
      case 1:
        return r.task().id();
      case 2:
        return r.statusAsString();
      case 3:
        return r.submissionDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
      case 4:
        return r.startDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
      case 5:
        return r.endDatetime().toString("yyyy-MM-dd hh:mm:ss,zzz");
      case 6:
        return r.startDatetime().isNull() || r.submissionDatetime().isNull()
            ? QVariant() : QString::number(r.queuedMillis()/1000.0);
      case 7:
        return r.endDatetime().isNull() || r.startDatetime().isNull()
            ? QVariant() : QString::number(r.runningMillis()/1000.0);
      case 8:
        return _customActions.isEmpty()
            ? QVariant() : r.params().evaluate(_customActions, &r);
      case 9:
        return r.abortable();
      }
      break;
    }
    default:
      ;
    }
  }
  return QVariant();
}

QVariant TaskInstancesModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      switch(section) {
      case 0:
        return "Instance Id";
      case 1:
        return "Fully qualified task name";
      case 2:
        return "Status";
      case 3:
        return "Submission";
      case 4:
        return "Start";
      case 5:
        return "End";
      case 6:
        return "Seconds queued";
      case 7:
        return "Seconds running";
      case 8:
        return "Actions";
      case 9:
        return "Abortable";
      }
    } else {
      return QString::number(section);
    }
  }
  return QVariant();
}

void TaskInstancesModel::taskChanged(TaskInstance instance) {
  int row;
  //Log::fatal() << "taskChanged " << instance.task().fqtn() << "/"
  //             << instance.id() << " " << instance.statusAsString();
  for (row = 0; row < _instances.size(); ++row) {
    TaskInstance &r(_instances[row]);
    if (r.id() == instance.id()) {
      //Log::fatal() << "TaskInstancesModel::taskChanged " << instance.id()
      //             << " found " << _keepFinished << " " << instance.finished();
      if (!instance.finished() || _keepFinished) {
        r = instance;
        emit dataChanged(index(row, 2), index(row, COLUMNS-1));
      } else {
        beginRemoveRows(QModelIndex(), row, row);
        _instances.removeAt(row);
        endRemoveRows();
      }
      return;
    }
    if (r.id() < instance.id())
      break;
  }
  //qDebug() << "TaskInstancesModel::taskChanged" << instance.id()
  //         << instance.submissionDatetime() << instance.startDatetime()
  //         << instance.endDatetime() << "new" << _keepFinished;
  if (!instance.finished() || _keepFinished) {
    // row should always be = 0 because signals from 1 thread to 1 other thread are ordered, however...
    beginInsertRows(QModelIndex(), row, row);
    _instances.insert(row, instance);
    endInsertRows();
  }
  if (_instances.size() > _maxrows) {
    beginRemoveRows(QModelIndex(), _maxrows, _instances.size());
    while (_instances.size() > _maxrows)
      _instances.removeAt(_maxrows);
    endRemoveRows();
  }
}
