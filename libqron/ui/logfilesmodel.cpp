/* Copyright 2013 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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
#include "logfilesmodel.h"

#define COLUMNS 3

LogFilesModel::LogFilesModel(QObject *parent)
  : QAbstractTableModel(parent) {
}

int LogFilesModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _logfiles.size();
}

int LogFilesModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant LogFilesModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _logfiles.size()) {
    LogFile logfile = _logfiles.at(index.row());
    switch(role) {
    case Qt::DisplayRole:
      switch(index.column()) {
      case 0:
        return logfile.pathPattern();
      case 1:
        return Log::severityToString(logfile.minimumSeverity());
      case 2:
        return logfile.buffered();
      }
      break;
    default:
      ;
    }
  }
  return QVariant();
}

QVariant LogFilesModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Path pattern";
    case 1:
      return "Minimum severity";
    case 2:
      return "Buffered";
    }
  }
  return QVariant();
}

void LogFilesModel::logConfigurationChanged(QList<LogFile> logfiles) {
  if (!_logfiles.isEmpty()) {
    beginRemoveRows(QModelIndex(), 0, _logfiles.size()-1);
    _logfiles.clear();
    endRemoveRows();
  }
  if (!logfiles.isEmpty()) {
    beginInsertRows(QModelIndex(), 0, logfiles.size()-1);
    _logfiles = logfiles;
    endInsertRows();
  }
}
