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
  "Balancing Method",
  "Label",
};

class ClusterData : public SharedUiItemData {
public:
  QString _id, _label;
  Cluster::Balancing _balancing;
  QList<Host> _hosts;
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const;
  int uiSectionCount() const;
  QString id() const { return _id; }
  QString idQualifier() const { return "cluster"; }
  bool setUiData(int section, const QVariant &value, QString *errorString,
                 int role);
  Qt::ItemFlags uiFlags(int section) const;
};

Cluster::Cluster() {
}

Cluster::Cluster(const Cluster &other) : SharedUiItem(other) {
}

Cluster::Cluster(PfNode node) {
  ClusterData *hgd = new ClusterData;
  hgd->_id = ConfigUtils::sanitizeId(node.contentAsString(), true);
  hgd->_label = node.attribute("label");
  hgd->_balancing = balancingFromString(node.attribute("balancing", "first")
                                        .trimmed().toLower());
  if (hgd->_balancing == UnknownBalancing) {
    Log::error() << "invalid cluster balancing method: " << node.toString();
    delete hgd;
  }
  setData(hgd);
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

Cluster::Balancing Cluster::balancing() const {
  return !isNull() ? cd()->_balancing : UnknownBalancing;
}

QString Cluster::label() const {
  const ClusterData *d = cd();
  return d ? (d->_label.isEmpty() ? d->_id : d->_label)
           : QString();
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
      return Cluster::balancingAsString(_balancing);
    case 3:
      return _label.isEmpty() ? _id : _label;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

bool ClusterData::setUiData(int section, const QVariant &value,
                            QString *errorString, int role) {
  if (role != Qt::EditRole) {
    if (errorString)
      *errorString = "cannot set other role than EditRole";
    return false;
  }
  QString s = value.toString().trimmed();
  switch(section) {
  case 0:
    if (s.isEmpty()) {
      if (errorString)
        *errorString = "id cannot be empty";
      return false;
    }
    _id = s;
    return true;
    //case 1:
    // TODO host list: parse
  case 2: {
    Cluster::Balancing balancing
        = Cluster::balancingFromString(value.toString().trimmed().toLower());
    if (balancing == Cluster::UnknownBalancing) {
      if (errorString)
        *errorString = "unsupported balancing value: '"+value.toString()+"'";
      return false;
    }
    _balancing = balancing;
    return true;
  }
  case 3:
    _label = s;
    return true;
  }
  if (errorString)
    *errorString = "field \""+uiHeaderData(section, Qt::DisplayRole).toString()
      +"\" is not ui-editable";
  return false;
}

Qt::ItemFlags ClusterData::uiFlags(int section) const {
  Qt::ItemFlags flags = SharedUiItemData::uiFlags(section);
  switch(section) {
  case 0:
  case 2:
  case 3:
    flags |= Qt::ItemIsEditable;
  }
  return flags;
}

bool Cluster::setUiData(int section, const QVariant &value,
                        QString *errorString, int role) {
  if (isNull())
    return false;
  detach<ClusterData>();
  return ((ClusterData*)constData())
      ->setUiData(section, value, errorString, role);
}

QVariant ClusterData::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int ClusterData::uiSectionCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

ClusterData *Cluster::cd() {
  detach<ClusterData>();
  return (ClusterData*)constData();
}

PfNode Cluster::toPfNode() const {
  const ClusterData *cd = this->cd();
  if (!cd)
    return PfNode();
  PfNode node("cluster", cd->_id);
  if (!cd->_label.isEmpty() && cd->_label != cd->_id)
    node.appendChild(PfNode("label", cd->_label));
  node.appendChild(PfNode("balancing", balancingAsString(cd->_balancing)));
  QStringList hosts;
  foreach (const Host &host, cd->_hosts)
    hosts.append(host.id());
  node.appendChild(PfNode("hosts", hosts.join(' ')));
  return node;
}

void Cluster::setHosts(QList<Host> hosts) {
  ClusterData *d = cd();
  if (d)
    d->_hosts = hosts;
}

Cluster::Balancing Cluster::balancingFromString(QString method) {
  if (method == "first")
    return First;
  if (method == "each")
    return Each;
  return UnknownBalancing;
}

QString Cluster::balancingAsString(Cluster::Balancing method) {
  // LATER optimize with const QString
  switch (method) {
  case First:
    return "first";
  case Each:
    return "each";
  case UnknownBalancing:
    ;
  }
  return QString();
}
