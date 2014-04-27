/* Copyright 2012-2013 Hallowyn and others.
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
#include "cluster.h"
#include <QSharedData>
#include <QString>
#include "host.h"
#include <QList>
#include "pf/pfnode.h"
#include "log/log.h"
#include "configutils.h"

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Hosts",
  "Balancing method",
  "Label",
};

class ClusterData : public SharedUiItemData {
public:
  QString _id, _label, _balancing;
  QList<Host> _hosts;
  QVariant uiData(int section, int role) const;
  QString id() const { return _id; }
  QString idQualifier() const { return "cluster"; }
};

Cluster::Cluster() {
}

Cluster::Cluster(const Cluster &other) : SharedUiItem(other) {
}

Cluster::Cluster(PfNode node) {
  ClusterData *hgd = new ClusterData;
  hgd->_id = ConfigUtils::sanitizeId(node.contentAsString(), true);
  hgd->_label = node.attribute("label", hgd->_id);
  hgd->_balancing = node.attribute("balancing", "first");
  if (hgd->_balancing == "first" || hgd->_balancing == "each")
    setData(hgd);
  else {
    Log::error() << "invalid cluster balancing method '" << hgd->_balancing
                 << "': " << node.toString();
    delete hgd;
  }
}

Cluster::~Cluster() {
}

void Cluster::appendHost(Host host) {
  if (!isNull())
    cd()->_hosts.append(host);
}

QList<Host> Cluster::hosts() const {
  return !isNull() ? cd()->_hosts : QList<Host>();
}

QString Cluster::balancing() const {
  return !isNull() ? cd()->_balancing : QString();
}

QString Cluster::label() const {
  return !isNull() ? cd()->_label : QString();
}

void Cluster::setId(QString id) {
  if (!isNull())
    cd()->_id = id;
}

QVariant ClusterData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
    switch(section) {
    case 0:
      return _id;
    case 1: {
      QStringList hosts;
      foreach (Host h, _hosts)
        hosts.append(h.id());
      return hosts.join(" ");
    }
    case 2:
      return _balancing;
    case 3:
      return _label;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

QVariant Cluster::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int Cluster::uiDataCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

ClusterData *Cluster::cd() {
  detach<ClusterData>();
  return (ClusterData*)constData();
}
