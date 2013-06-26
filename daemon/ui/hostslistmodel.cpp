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
#include "hostslistmodel.h"
#include <QtDebug>
#include <QStringList>

#define COLUMNS 3

HostsListModel::HostsListModel(QObject *parent) : QAbstractListModel(parent) {
}

int HostsListModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _hosts.size();
}

int HostsListModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant HostsListModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _hosts.size()) {
    Host h = _hosts.at(index.row());
    switch(role) {
    case Qt::DisplayRole:
      switch(index.column()) {
      case 0:
        return h.id();
      case 1:
        return h.hostname();
      case 2:
        return h.resourcesAsString();
      }
      break;
    case TextViews::HtmlPrefixRole:
      // LATER move icon to WebConsole
      if (index.column() == 0)
        return "<i class=\"icon-hdd\"></i> ";
      break;
    default:
      ;
    }
  }
  return QVariant();
}

QVariant HostsListModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Id";
    case 1:
      return "Hostname";
    case 2:
      return "Resources";
    }
  }
  return QVariant();
}

void HostsListModel::setAllHostsAndClusters(QHash<QString, Cluster> clusters,
                                            QHash<QString, Host> hosts) {
  Q_UNUSED(clusters)
  beginResetModel();
  QStringList names;
  foreach(QString id, hosts.keys())
    names << id;
  names.sort(); // get a sorted hosts id list
  _hosts.clear();
  foreach(QString id, names)
    _hosts.append(hosts.value(id));
  endResetModel();
}
