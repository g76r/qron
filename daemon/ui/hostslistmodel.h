#ifndef HOSTSLISTMODEL_H
#define HOSTSLISTMODEL_H

#include <QAbstractListModel>
#include "textviews.h"
#include "data/cluster.h"
#include "data/host.h"

class HostsListModel : public QAbstractListModel {
  Q_OBJECT
  QList<Host> _hosts;

public:
  explicit HostsListModel(QObject *parent = 0);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
  void setAllHostsAndClusters(QMap<QString,Cluster> clusters,
                              QMap<QString,Host> hosts);
};

#endif // HOSTSLISTMODEL_H
