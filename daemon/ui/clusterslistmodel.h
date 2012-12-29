#ifndef CLUSTERSLISTMODEL_H
#define CLUSTERSLISTMODEL_H

#include <QAbstractListModel>
#include "textviews.h"
#include "data/cluster.h"
#include "data/host.h"

class ClustersListModel : public QAbstractListModel {
  Q_OBJECT
  QList<Cluster> _clusters;

public:
  explicit ClustersListModel(QObject *parent = 0);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
  void setAllHostsAndClusters(QMap<QString,Cluster> clusters,
                              QMap<QString,Host> hosts);
};

#endif // CLUSTERSLISTMODEL_H
