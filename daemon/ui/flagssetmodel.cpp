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
#include "flagssetmodel.h"

#define COLUMNS 2

FlagsSetModel::FlagsSetModel(QObject *parent) : QAbstractListModel(parent) {
}

int FlagsSetModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : _setFlags.size();
}

int FlagsSetModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant FlagsSetModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0
      && index.row() < _setFlags.size()) {
    if (role == Qt::DisplayRole) {
      const SetFlag &sf(_setFlags.at(index.row()));
      switch(index.column()) {
      case 0:
        return sf._flag;
      case 1:
        return sf._setTime;
      }
    } else if (role == _prefixRole && index.column() == 1)
      return _prefix;
  }
  return QVariant();
}

QVariant FlagsSetModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      switch(section) {
      case 0:
        return "Flag";
      case 1:
        return "Set on";
      }
    } else {
      return QString::number(section);
    }
  }
  return QVariant();
}

void FlagsSetModel::setFlag(QString flag) {
  int row;
  for (row = 0; row < _setFlags.size(); ++row) {
    const SetFlag &sf(_setFlags.at(row));
    if (sf._flag == flag)
      return;
  }
  for (row = 0; row < _setFlags.size(); ++row) {
    const SetFlag &sf(_setFlags.at(row));
    if (sf._flag > flag)
      break;
  }
  beginInsertRows(QModelIndex(), row, row);
  _setFlags.insert(row, SetFlag(flag));
  endInsertRows();
}

void FlagsSetModel::clearFlag(QString alert) {
  for (int row = 0; row < _setFlags.size(); ++row) {
    const SetFlag &sf(_setFlags.at(row));
    if (sf._flag == alert) {
      beginRemoveRows(QModelIndex(), row, row);
      _setFlags.removeAt(row);
      endRemoveRows();
      break;
    }
  }
}
