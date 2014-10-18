/* Copyright 2013-2014 Hallowyn and others.
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
#include "calendarsmodel.h"

#define COLUMNS 2

CalendarsModel::CalendarsModel(QObject *parent) : QAbstractTableModel(parent) {
}

int CalendarsModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : _calendars.size();
}

int CalendarsModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant CalendarsModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _calendars.size()) {
    const Calendar c(_calendars.at(index.row()));
    switch(role) {
    case Qt::DisplayRole:
      switch(index.column()) {
      case 0:
        return c.name();
      case 1:
        return c.toCommaSeparatedRulesString();
      }
      break;
    default:
      ;
    }
  }
  return QVariant();
}

QVariant CalendarsModel::headerData(int section, Qt::Orientation orientation,
                                int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Name";
    case 1:
      return "Date rules";
    }
  }
  return QVariant();
}

void CalendarsModel::configChanged(SchedulerConfig config) {
  if (!_calendars.isEmpty()) {
    beginRemoveRows(QModelIndex(), 0, _calendars.size()-1);
    _calendars.clear();
    endRemoveRows();
  }
  QStringList names = config.namedCalendars().keys();
  names.sort();
  if (!names.isEmpty()) {
    beginInsertRows(QModelIndex(), 0, names.size()-1);
    foreach (QString name, names)
      _calendars.append(config.namedCalendars().value(name));
    endInsertRows();
  }
}
