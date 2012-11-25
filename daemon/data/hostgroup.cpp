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
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "hostgroup.h"
#include <QSharedData>
#include <QString>
#include "host.h"
#include <QList>
#include "pfnode.h"

class HostGroupData : public QSharedData {
  friend class HostGroup;
  QString _id, _label;
  QList<Host> _hosts;
public:
};

HostGroup::HostGroup() : d(new HostGroupData) {
}

HostGroup::HostGroup(const HostGroup &other) : d(other.d) {
}

HostGroup::HostGroup(PfNode node) {
  HostGroupData *hgd = new HostGroupData;
  hgd->_id = node.attribute("id"); // LATER check uniqueness
  hgd->_label = node.attribute("label", hgd->_id);
  d = hgd;
}


HostGroup::~HostGroup() {
}

HostGroup &HostGroup::operator=(const HostGroup &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

void HostGroup::appendHost(Host host) {
  d->_hosts.append(host);
}

const QList<Host> HostGroup::hosts() const {
  return d->_hosts;
}

QString HostGroup::id() const {
  return d->_id;
}
