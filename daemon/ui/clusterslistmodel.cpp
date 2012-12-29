#include "clusterslistmodel.h"
#include <QtDebug>
#include <QStringList>

#define COLUMNS 3

ClustersListModel::ClustersListModel(QObject *parent)
  : QAbstractListModel(parent) {
}

int ClustersListModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return _clusters.size();
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
        return c.method();
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
