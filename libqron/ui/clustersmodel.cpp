/* Copyright 2012-2015 Hallowyn and others.
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
#include "clustersmodel.h"
#include <QStringList>
#include <QMimeData>
#include "modelview/shareduiitemdocumentmanager.h"

/** Host reference item, to be inserted as cluster child in cluster tree,
 * without the need of having real host items which data are inconsistent with
 * cluster data */
class HostReference : public SharedUiItem {
  class HostReferenceData : public SharedUiItemData {
  public:
    QString _id, _cluster, _host;

    HostReferenceData() { }
    HostReferenceData(QString cluster, QString host)
      : _id(cluster+"~"+host), _cluster(cluster), _host(host) { }
    QString id() const { return _id; }
    QString idQualifier() const { return QStringLiteral("hostreference"); }
    int uiSectionCount() const { return 1; }
    QVariant uiData(int section, int role) const {
      return section == 0 && role == Qt::DisplayRole ? _host : QVariant(); }
    QVariant uiHeaderData(int section, int role) const {
      return section == 0 && role == Qt::DisplayRole
          ? QStringLiteral("Host") : QVariant(); }
  };

public:
  HostReference() : SharedUiItem() { }
  explicit HostReference(QString cluster, QString host)
    : SharedUiItem(new HostReferenceData(cluster, host)) { }
  HostReference(const HostReference &other)
    : SharedUiItem(other) { }
  HostReference &operator=(const HostReference &other) {
    SharedUiItem::operator=(other); return *this; }
  QString cluster() const { return isNull() ? QString() : data()->_cluster; }

private:
  const HostReferenceData *data() const {
    return (const HostReferenceData*)SharedUiItem::data(); }
};


ClustersModel::ClustersModel(QObject *parent)
  : SharedUiItemsTreeModel(parent) {
  setHeaderDataFromTemplate(Cluster(PfNode("template")));
}

void ClustersModel::changeItem(
    SharedUiItem newItem, SharedUiItem oldItem, QString idQualifier) {
  if (idQualifier == QStringLiteral("cluster")) {
    // remove host references rows
    QModelIndex oldIndex = indexOf(oldItem);
    if (oldIndex.isValid())
      removeRows(0, rowCount(oldIndex), oldIndex);
    // regular changeItem
    SharedUiItemsTreeModel::changeItem(newItem, oldItem, idQualifier);
    // insert host references rows
    QModelIndex newIndex = indexOf(newItem);
    if (newIndex.isValid()) {
      Cluster &cluster = reinterpret_cast<Cluster&>(newItem);
      SharedUiItem nullItem;
      foreach (const Host &host, cluster.hosts()) {
        SharedUiItemsTreeModel::changeItem(
              HostReference(cluster.id(), host.id()), nullItem,
              QStringLiteral("hostreference"));
      }
    }
  } else {
    SharedUiItemsTreeModel::changeItem(newItem, oldItem, idQualifier);
  }
}

void ClustersModel::determineItemPlaceInTree(
    SharedUiItem newItem, QModelIndex *parent, int *row) {
  Q_UNUSED(row)
  if (newItem.idQualifier() == "hostreference") {
    HostReference &hf = reinterpret_cast<HostReference&>(newItem);
    *parent = indexOf("cluster:"+hf.cluster());
  }
}

bool ClustersModel::canDropMimeData(
    const QMimeData *data, Qt::DropAction action, int targetRow,
    int targetColumn, const QModelIndex &targetParent) const {
  Q_UNUSED(action)
  Q_UNUSED(targetRow)
  Q_UNUSED(targetColumn)
  if (!documentManager())
    return false; // cannot change data w/o dm
  if (!targetParent.isValid())
    return false; // cannot drop on root
  QList<QByteArray> idsArrays =
      data->data(suiQualifiedIdsListMimeType).split(' ');
  if (idsArrays.isEmpty())
    return false; // nothing to drop
  foreach (const QByteArray &qualifiedId, idsArrays) {
    QString idQualifier = QString::fromUtf8(
          qualifiedId.left(qualifiedId.indexOf(':')));
    if (idQualifier != QStringLiteral("host")
        && idQualifier != QStringLiteral("hostreference"))
      return false; // can only drop hosts
  }
  return true;
}

// Dropping on ClustersModel provide mean to reorder hosts of a given cluster
// and/or to add hosts to it.
// Therefore we support dropping a mix of hosts and hostreferences and make no
// assumption of whether they already belong to targeted cluster or not.
bool ClustersModel::dropMimeData(
    const QMimeData *data, Qt::DropAction action, int targetRow,
    int targetColumn, const QModelIndex &targetParent) {
  Q_UNUSED(action)
  Q_UNUSED(targetColumn)
  //qDebug() << "ClustersModel::dropMimeData";
  // find cluster to modify
  QModelIndex clusterIndex;
  if (targetParent.parent().isValid()) {
    clusterIndex = targetParent.parent();
    targetRow = targetParent.row();
  } else {
    clusterIndex = targetParent;
    targetRow = 0;
  }
  SharedUiItem clusterSui = itemAt(clusterIndex);
  Cluster &oldCluster = static_cast<Cluster&>(clusterSui);
  //qDebug() << "  dropping on:" << oldCluster.id() << targetRow;
  // build new hosts id list
  QList<QByteArray> idsArrays =
      data->data(suiQualifiedIdsListMimeType).split(' ');
  QStringList oldHostsIds, droppedHostsIds, newHostsIds;
  foreach (const Host &host, oldCluster.hosts())
    oldHostsIds << host.id();
  foreach (const QByteArray &qualifiedId, idsArrays) {
    QByteArray hostId;
    if (qualifiedId.contains('~'))
      hostId = qualifiedId.mid(qualifiedId.indexOf('~')+1);
    else
      hostId = qualifiedId.mid(qualifiedId.indexOf(':')+1);
    droppedHostsIds += QString::fromUtf8(hostId);
  }
  int oldIndex = 0;
  for (; oldIndex < targetRow && oldIndex < oldHostsIds.size(); ++oldIndex) {
    const QString &id = oldHostsIds[oldIndex];
    if (!droppedHostsIds.contains(id))
      newHostsIds << id;
  }
  for (int droppedIndex = 0; droppedIndex < droppedHostsIds.size();
       ++droppedIndex)
    newHostsIds << droppedHostsIds[droppedIndex];
  for (; oldIndex < oldHostsIds.size(); ++oldIndex) {
    const QString &id = oldHostsIds[oldIndex];
    if (!droppedHostsIds.contains(id))
      newHostsIds << id;
  }
  //qDebug() << "  new hosts list:" << newHostsIds;
  //qDebug() << "  old one:" << oldHostsIds << "dropped one:" << droppedHostsIds;
  // update actual data item
  QList<Host> newHosts;
  foreach (const QString &id, newHostsIds) {
    SharedUiItem hostSui =
        documentManager()->itemById(QStringLiteral("host"), id);
    Host &host = static_cast<Host&>(hostSui);
    newHosts << host;
  }
  Cluster newCluster = oldCluster;
  newCluster.setHosts(newHosts);
  documentManager()->changeItem(newCluster, oldCluster,
                                QStringLiteral("cluster"));
  return true;
}
