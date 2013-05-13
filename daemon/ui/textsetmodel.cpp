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
#include "textsetmodel.h"

TextSetModel::TextSetModel(QObject *parent) : QAbstractListModel(parent) {
}

int TextSetModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _values.size();
}

QVariant TextSetModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _values.size()
      && role == Qt::DisplayRole && index.column() == 0)
    return  _values.at(index.row());
  return QVariant();
}

void TextSetModel::insertValue(const QString value) {
  // LATER optimize TextSetModel::insertValue
  // see e.g. FlagsSetModel::setFlag or use QMap instead of QList
  if (!_values.contains(value)) {
    beginResetModel();
    _values.append(value);
    _values.sort();
    endResetModel();
  }
}

void TextSetModel::removeValue(const QString value) {
  // LATER optimize TextSetModel::removeValue
  // see e.g. FlagsSetModel::setFlag or use QMap instead of QList
  if (_values.contains(value)) {
    beginResetModel();
    _values.removeOne(value);
    endResetModel();
  }
}
