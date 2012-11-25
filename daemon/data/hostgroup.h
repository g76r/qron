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
#ifndef HOSTGROUP_H
#define HOSTGROUP_H

#include <QSharedDataPointer>
#include <QList>

class HostGroupData;
class Host;
class PfNode;

class HostGroup {
  QSharedDataPointer<HostGroupData> d;

public:
  HostGroup();
  HostGroup(const HostGroup &other);
  HostGroup(PfNode node);
  ~HostGroup();
  HostGroup &operator=(const HostGroup &other);
  void appendHost(Host host);
  const QList<Host> hosts() const;
  QString id() const;
};

#endif // HOSTGROUP_H
