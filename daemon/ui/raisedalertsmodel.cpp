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
#include "raisedalertsmodel.h"
#include <QtDebug>

#define COLUMNS 3

RaisedAlertsModel::RaisedAlertsModel(QObject *parent)
  : QAbstractListModel(parent) {
}

int RaisedAlertsModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _raisedAlerts.size();
}

int RaisedAlertsModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant RaisedAlertsModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0
      && index.row() < _raisedAlerts.size()) {
    switch(role) {
    case Qt::DisplayRole: {
      const RaisedAlert ra(_raisedAlerts.at(index.row()));
      switch(index.column()) {
      case 0:
        return ra._alert;
      case 1:
        return ra._raiseTime;
      case 2:
        return ra._scheduledCancellationTime;
      }
      break;
    }
    default:
      ;
    }
  }
  return QVariant();
}

QVariant RaisedAlertsModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      switch(section) {
      case 0:
        return "Alert";
      case 1:
        return "Raised on";
      case 2:
        return "Cancellation scheduled on";
      }
    } else {
      return QString::number(section);
    }
  }
  // LATER htmlPrefix <i class="icon-bell"></i>
  return QVariant();
}

void RaisedAlertsModel::alertRaised(QString alert) {
  int row;
  for (row = 0; row < _raisedAlerts.size(); ++row) {
    RaisedAlert ra(_raisedAlerts.at(row));
    if (ra._alert == alert) {
      // this should not occur since a not yet canceled alert should never
      // be raised again
      ra._raiseTime = QDateTime::currentDateTime();
      ra._scheduledCancellationTime = QDateTime();
      emit dataChanged(index(row, 1), index(row, 2));
      return;
    }
  }
  for (row = 0; row < _raisedAlerts.size(); ++row) {
    const RaisedAlert ra(_raisedAlerts.at(row));
    if (ra._alert > alert)
      break;
  }
  beginInsertRows(QModelIndex(), row, row);
  _raisedAlerts.insert(row, RaisedAlert(alert));
  endInsertRows();
}

void RaisedAlertsModel::alertCanceled(QString alert) {
  for (int row = 0; row < _raisedAlerts.size(); ++row) {
    const RaisedAlert ra(_raisedAlerts.at(row));
    if (ra._alert == alert) {
      beginRemoveRows(QModelIndex(), row, row);
      _raisedAlerts.removeAt(row);
      endRemoveRows();
      break;
    }
  }
}

void RaisedAlertsModel::alertCancellationScheduled(QString alert,
                                                   QDateTime scheduledTime) {
  for (int row = 0; row < _raisedAlerts.size(); ++row) {
    RaisedAlert ra(_raisedAlerts.at(row));
    if (ra._alert == alert) {
      ra._scheduledCancellationTime = scheduledTime;
      _raisedAlerts.replace(row, ra);
      QModelIndex i = index(row, 2);
      emit dataChanged(i, i);
      break;
    }
  }
}
