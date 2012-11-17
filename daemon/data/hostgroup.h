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
