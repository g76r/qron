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
#include "taskrequestsmodel.h"

#define COLUMNS 10

TaskRequestsModel::TaskRequestsModel(QObject *parent, int maxrows,
                                     bool keepFinished)
  : QAbstractListModel(parent), _maxrows(maxrows), _keepFinished(keepFinished) {
}

int TaskRequestsModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _requests.size();
}

int TaskRequestsModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant TaskRequestsModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _requests.size()) {
    const TaskRequest r(_requests.at(index.row()));
    switch(role) {
    case Qt::DisplayRole: {
      switch(index.column()) {
      case 0:
        return r.id();
      case 1:
        return r.task().fqtn();
      case 2:
        if (!r.endDatetime().isNull())
          return r.success() ? "success" : "failure";
        if (!r.startDatetime().isNull())
          return "running";
        if (r.task().enabled())
          return "queued";
        return "queued and disabled";
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
        return QString::number(r.task().instancesCount())+" / "
            +QString::number(r.task().maxInstances());
      }
      break;
    }
    case TextViews::HtmlPrefixRole:
      switch(index.column()) {
      case 2: {
        if (!r.endDatetime().isNull())
          return r.success() ? QVariant()
                             : "<i class=\"icon-minus-sign\"></i> ";
        if (!r.startDatetime().isNull())
          return "<i class=\"icon-play\"></i> ";
        if (r.task().enabled())
          return "<i class=\"icon-time\"></i> ";
        return "<i class=\"icon-ban-circle\"></i> ";
      }
      case 9: {
        QString actions;
        actions = " <span class=\"label label-info\" title=\"Log\">"
            "<a target=\"_blank\" href=\"/rest/txt/log/all/v1?filter=%20"
            +r.task().fqtn()+"/"+QString::number(r.id())
            +"%20\"><i class=\"icon-search icon-white\"></i></a></span>";
        return actions;
      }
      default:
        ;
      }
      break;
    case TextViews::TrClassRole:
      if (!r.endDatetime().isNull())
        return r.success() ? QVariant() : "error";
      if (!r.startDatetime().isNull())
        return "info";
      return "warning";
    case TextViews::HtmlSuffixRole:
      switch(index.column()) {
      case 9: {
        QString infourl = r.task().infourl();
        if (!infourl.isEmpty())
          return " <span class=\"label label-info\" "
              "title=\"Information / Documentation\"><a target=\"_blank\" "
              "href=\""+infourl+"\"><i class=\"icon-info-sign icon-white\">"
              "</i></a></span>";
      }
      default:
        ;
      }
      break;
    default:
      ;
    }
  }
  return QVariant();
}

QVariant TaskRequestsModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      switch(section) {
      case 0:
        return "Request Id";
      case 1:
        return "Task Id";
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
        return "Instances / max";
      case 9:
        return "Actions";
      }
    } else {
      return QString::number(section);
    }
  }
  return QVariant();
}

void TaskRequestsModel::taskChanged(TaskRequest request) {
  int row;
  for (row = 0; row < _requests.size(); ++row) {
    TaskRequest &r(_requests[row]);
    if (r.id() == request.id()) {
      //qDebug() << "TaskRequestsModel::taskChanged" << request.id()
      //         << request.submissionDatetime() << request.startDatetime()
      //         << request.endDatetime() << "found" << _keepFinished;
      if (r.endDatetime().isNull() || _keepFinished) {
        r = request;
        emit dataChanged(index(row, 1), index(row, 5));
      } else {
        beginRemoveRows(QModelIndex(), row, row);
        _requests.removeAt(row);
        endRemoveRows();
      }
      return;
    }
  }
  //qDebug() << "TaskRequestsModel::taskChanged" << request.id()
  //         << request.submissionDatetime() << request.startDatetime()
  //         << request.endDatetime() << "new" << _keepFinished;
  if (request.endDatetime().isNull() || _keepFinished) {
    beginInsertRows(QModelIndex(), 0, 0);
    _requests.prepend(request);
    endInsertRows();
  }
  if (_requests.size() > _maxrows) {
    beginRemoveRows(QModelIndex(), _maxrows, _requests.size());
    while (_requests.size() > _maxrows)
      _requests.removeAt(_maxrows);
    endRemoveRows();
  }
}
