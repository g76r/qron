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
#ifndef CLUSTER_H
#define CLUSTER_H

#include <QSharedDataPointer>
#include <QList>

class ClusterData;
class Host;
class PfNode;

class Cluster {
  QSharedDataPointer<ClusterData> d;

public:
  Cluster();
  Cluster(const Cluster &other);
  Cluster(PfNode node);
  ~Cluster();
  Cluster &operator=(const Cluster &other);
  bool isNull() const;
  bool operator==(const Cluster &other) const;
  bool operator<(const Cluster &other) const;
  void appendHost(Host host);
  QList<Host> hosts() const;
  QString id() const;
  QString balancing() const;
};

#endif // CLUSTER_H
