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

class ClusterData : public QSharedData {
public:
  QString _id, _label, _balancing;
  QList<Host> _hosts;
};

Cluster::Cluster() : d(new ClusterData) {
}

Cluster::Cluster(const Cluster &other) : d(other.d) {
}

Cluster::Cluster(PfNode node) {
  ClusterData *hgd = new ClusterData;
  hgd->_id = ConfigUtils::sanitizeId(node.contentAsString(), true);
  hgd->_label = node.attribute("label", hgd->_id);
  hgd->_balancing = node.attribute("balancing", "first");
  if (hgd->_balancing == "first" || hgd->_balancing == "each")
    d = hgd;
  else {
    Log::error() << "invalid cluster balancing method '" << hgd->_balancing
                 << "': " << node.toString();
    delete hgd;
  }
}

Cluster::~Cluster() {
}

Cluster &Cluster::operator=(const Cluster &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

bool Cluster::isNull() const {
  return !d;
}

bool Cluster::operator==(const Cluster &other) const {
  return id() == other.id();
}

bool Cluster::operator<(const Cluster &other) const {
  return id() < other.id();
}

void Cluster::appendHost(Host host) {
  if (d)
    d->_hosts.append(host);
}

QList<Host> Cluster::hosts() const {
  return d ? d->_hosts : QList<Host>();
}

QString Cluster::id() const {
  return d ? d->_id : QString();
}

void Cluster::setId(QString id) {
  if (d)
    d->_id = id;
}

QString Cluster::balancing() const {
  return d ? d->_balancing : QString();
}
