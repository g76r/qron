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
#include "targetstreemodel.h"
#include <QtDebug>
#include <QStringList>

#define COLUMNS 3

TargetsTreeModel::TargetsTreeModel(QObject *parent) :
  TreeModelWithStructure(parent) {
  _hostsItem = new TreeItem(_root, "Hosts", "hosts", 1, true);
  _clustersItem = new TreeItem(_root, "Clusters", "clusters", 1, true);
}

int TargetsTreeModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant TargetsTreeModel::data(const QModelIndex &index, int role) const {
  //qDebug() << "TasksTreeModel::data()" << index << index.isValid()
  //         << index.internalPointer();
  if (index.isValid()) {
    TreeItem *i = (TreeItem*)(index.internalPointer());
    //qDebug() << "  " << i;
    if (i) {
      //qDebug() << "  " << i << i->_isStructure << i->_id << i->_path;
      if (i->_isStructure) {
        Cluster g = _clusters.value(i->_path);
        switch(role) {
        case Qt::DisplayRole:
          switch(index.column()) {
          case 0:
            return i->_id;
          }
          break;
        case HtmlPrefixRole:
          if (index.column() == 0)
            return "<i class=\"icon-folder-open\"></i> ";
          break;
        default:
          ;
        }
      } else if (i->_parent == _clustersItem){
        // cluster
        Cluster c = _clusters.value(i->_id);
        switch(role) {
        case Qt::DisplayRole:
          switch(index.column()) {
          case 0:
            return i->_id;
          case 1: {
            QStringList hosts;
            foreach (Host h, c.hosts())
              hosts.append(h.id());
            return hosts.join(" ");
          } case 2:
            return c.method();
          }
          break;
        case HtmlPrefixRole:
          if (index.column() == 0)
            return "<i class=\"icon-random\"></i> ";
          break;
        default:
          ;
        }
      } else {
        // host
        Host h = _hosts.value(i->_id);
        switch(role) {
        case Qt::DisplayRole:
          switch(index.column()) {
          case 0:
            return i->_id;
          case 1:
            return h.hostname();
          case 2:
            return h.resourcesAsString();
          }
          break;
        case HtmlPrefixRole:
          if (index.column() == 0)
            return "<i class=\"icon-hdd\"></i> ";
          // icon-random
          break;
        default:
          ;
        }
      }
    }
  }
  return QVariant();
}

QVariant TargetsTreeModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Id";
    case 1:
      return "Hostname/Hosts";
    case 2:
      return "Resources/Method";
    }
  }
  return QVariant();
}

void TargetsTreeModel::setAllHostsAndGroups(QMap<QString, Cluster> clusters, QMap<QString, Host> hosts) {
  beginResetModel();
  QStringList names;
  foreach(QString id, clusters.keys())
    names << id;
  names.sort(); // get a sorted clusters id list
  foreach(QString id, names)
    new TreeItem(_clustersItem, id, "clusters."+id, 2, false);
  names.clear();
  foreach(QString id, hosts.keys())
    names << id;
  names.sort(); // get a sorted hosts id list
  foreach(QString id, names)
    new TreeItem(_hostsItem, id, "hosts."+id, 2, false);
  _clusters = clusters;
  _hosts = hosts;
  endResetModel();
}
