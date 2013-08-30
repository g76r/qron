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
#include "cluster.h"
#include <QSharedData>
#include <QString>
#include "host.h"
#include <QList>
#include "pf/pfnode.h"

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
  hgd->_id = node.attribute("id"); // LATER check uniqueness
  hgd->_label = node.attribute("label", hgd->_id);
  hgd->_balancing = node.attribute("balancing", "first"); // LATER check validity
  d = hgd;
}

Cluster::~Cluster() {
}

Cluster &Cluster::operator=(const Cluster &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

bool Cluster::operator==(const Cluster &other) const {
  return id() == other.id();
}

bool Cluster::operator<(const Cluster &other) const {
  return id() < other.id();
}

void Cluster::appendHost(Host host) {
  d->_hosts.append(host);
}

const QList<Host> Cluster::hosts() const {
  return d->_hosts;
}

QString Cluster::id() const {
  return d->_id;
}

QString Cluster::balancing() const {
  return d->_balancing;
}
