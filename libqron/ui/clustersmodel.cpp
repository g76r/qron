/* Copyright 2012-2015 Hallowyn and others.
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
#include "clustersmodel.h"
#include <QtDebug>
#include <QStringList>

/** Host reference item, to be inserted as cluster child in cluster tree,
 * without the need of having real host items which data are inconsistent with
 * cluster data */
class HostReference : public SharedUiItem {
  class HostReferenceData : public SharedUiItemData {
  public:
    QString _cluster, _host;

    HostReferenceData() { }
    HostReferenceData(QString cluster, QString host)
      : _cluster(cluster), _host(host) { }
    QString id() const { return _cluster+"-"+_host; }
    QString idQualifier() const { return "hostreference"; }
    int uiSectionCount() const { return 1; }
    QVariant uiData(int section, int role) const {
      return section == 0 && role == Qt::DisplayRole ? _host : QVariant(); }
    QVariant uiHeaderData(int section, int role) const {
      return section == 0 && role == Qt::DisplayRole ? "Host" : QVariant(); }
  };

public:
  HostReference() : SharedUiItem() { }
  explicit HostReference(QString cluster, QString host)
    : SharedUiItem(new HostReferenceData(cluster, host)) { }
  HostReference(const HostReference &other)
    : SharedUiItem(other) { }
  HostReference &operator=(const HostReference &other) {
    SharedUiItem::operator=(other); return *this; }
  QString cluster() const { return isNull() ? QString() : hrd()->_cluster; }

private:
  const HostReferenceData *hrd() const {
    return (const HostReferenceData*)constData(); }
};


ClustersModel::ClustersModel(QObject *parent)
  : SharedUiItemsTreeModel(parent) {
  setHeaderDataFromTemplate(Cluster(PfNode("template")));
}

void ClustersModel::configReset(SchedulerConfig config) {
  clear();
  SharedUiItem nullItem;
  foreach(const Cluster &cluster, config.clusters())
    changeItem(cluster, nullItem);
}

void ClustersModel::changeItem(SharedUiItem newItem, SharedUiItem oldItem) {
  if (newItem.idQualifier() == "cluster"
      || oldItem.idQualifier() == "cluster") {
    // remove host references rows
    QModelIndex oldIndex = indexOf(oldItem);
    qDebug() << "ClustersModel::changeItem"
             << newItem.qualifiedId() << oldItem.qualifiedId()
             << "oldIndex:" << oldIndex << "oldRowCount:" << rowCount(oldIndex);
    if (oldIndex.isValid())
      removeRows(0, rowCount(oldIndex), oldIndex);
    // regular changeItem
    SharedUiItemsTreeModel::changeItem(newItem, oldItem);
    // insert host references rows
    QModelIndex newIndex = indexOf(newItem);
    if (newIndex.isValid()) {
      Cluster &cluster = reinterpret_cast<Cluster&>(newItem);
      SharedUiItem nullItem;
      foreach (const Host &host, cluster.hosts()) {
        SharedUiItemsTreeModel::changeItem(
              HostReference(cluster.id(), host.id()), nullItem);
      }
    }
    qDebug() << "/ClustersModel::changeItem";
  } else {
    SharedUiItemsTreeModel::changeItem(newItem, oldItem);
  }
}

void ClustersModel::setNewItemInsertionPoint(
    SharedUiItem newItem, QModelIndex *parent, int *row) {
  if (newItem.idQualifier() == "hostreference") {
    HostReference &hf = reinterpret_cast<HostReference&>(newItem);
    *parent = indexOf("cluster:"+hf.cluster());
    qDebug() << "ClustersModel::setNewItemInsertionPoint/hostreference"
             << hf.qualifiedId() << parent;
  }
}

