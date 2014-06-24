/* Copyright 2012-2014 Hallowyn and others.
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

#include "libqron_global.h"
#include <QSharedDataPointer>
#include <QList>
#include "host.h"

class ClusterData;
class PfNode;

/** A cluster is a multiple execution target.
 * It consist of a set of hosts grouped together as a target for load balancing,
 * failover or dispatching purposes.
 * @see Host */
class LIBQRONSHARED_EXPORT Cluster : public SharedUiItem {
public:
  Cluster();
  Cluster(const Cluster &other);
  Cluster(PfNode node);
  ~Cluster();
  Cluster &operator=(const Cluster &other) {
    SharedUiItem::operator=(other); return *this; }
  void appendHost(Host host);
  QList<Host> hosts() const;
  QString balancing() const;
  QString label() const;
  void setId(QString id);
  QVariant uiHeaderData(int section, int role) const;
  int uiDataCount() const;

private:
  ClusterData *cd();
  const ClusterData *cd() const { return (const ClusterData*)constData(); }
};

#endif // CLUSTER_H