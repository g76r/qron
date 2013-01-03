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
#include "lastemitedalertsmodel.h"
#include <QtDebug>

#define COLUMNS 2

LastEmitedAlertsModel::LastEmitedAlertsModel(QObject *parent, int maxsize)
  : QAbstractListModel(parent), _maxsize(maxsize) {
}

int LastEmitedAlertsModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _emitedAlerts.size();
}

int LastEmitedAlertsModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant LastEmitedAlertsModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0
      && index.row() < _emitedAlerts.size()) {
    switch(role) {
    case Qt::DisplayRole: {
      const EmitedAlert ea(_emitedAlerts.at(index.row()));
      switch(index.column()) {
      case 0:
        return ea._datetime;
      case 1:
        return ea._alert;
      }
      break;
    }
    default:
      ;
    }
  }
  return QVariant();
}

QVariant LastEmitedAlertsModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      switch(section) {
      case 0:
        return "Timestamp";
      case 1:
        return "Alert";
      }
    } else {
      return QString::number(section);
    }
  }
  // LATER htmlPrefix <i class="icon-bell"></i>
  return QVariant();
}

void LastEmitedAlertsModel::alertEmited(QString alert) {
  beginInsertRows(QModelIndex(), 0, 0);
  _emitedAlerts.prepend(EmitedAlert(alert));
  endInsertRows();
  if (_emitedAlerts.size() > _maxsize) {
    beginRemoveRows(QModelIndex(), _maxsize, _maxsize);
    _emitedAlerts.removeAt(_maxsize);
    endRemoveRows();
  }
}
