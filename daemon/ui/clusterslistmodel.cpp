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
#include "clusterslistmodel.h"
#include <QtDebug>
#include <QStringList>

#define COLUMNS 3

ClustersListModel::ClustersListModel(QObject *parent)
  : QAbstractListModel(parent) {
}

int ClustersListModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return parent.isValid() ? 0 : _clusters.size();
}

int ClustersListModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant ClustersListModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _clusters.size()) {
    Cluster c = _clusters.at(index.row());
    switch(role) {
    case Qt::DisplayRole:
      switch(index.column()) {
      case 0:
        return c.id();
      case 1: {
        QStringList hosts;
        foreach (Host h, c.hosts())
          hosts.append(h.id());
        return hosts.join(" ");
      } case 2:
        return c.balancing();
      }
      break;
    case TextViews::HtmlPrefixRole:
      if (index.column() == 0)
        return "<i class=\"icon-random\"></i> ";
      break;
    default:
      ;
    }
  }
  return QVariant();
}

QVariant ClustersListModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Id";
    case 1:
      return "Hosts";
    case 2:
      return "Balancing method";
    }
  }
  return QVariant();
}

void ClustersListModel::setAllHostsAndClusters(QMap<QString, Cluster> clusters,
                                               QMap<QString, Host> hosts) {
  Q_UNUSED(hosts)
  beginResetModel();
  QStringList names;
  foreach(QString id, clusters.keys())
    names << id;
  names.sort(); // get a sorted clusters id list
  _clusters.clear();
  foreach(QString id, names)
    _clusters.append(clusters.value(id));
  endResetModel();
}
