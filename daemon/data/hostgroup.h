#ifndef HOSTGROUP_H
#define HOSTGROUP_H

#include <QSharedDataPointer>

class HostGroupData;

class HostGroup {
  QSharedDataPointer<HostGroupData> d;

public:
  HostGroup();
  HostGroup(const HostGroup &other);
  ~HostGroup();
  HostGroup &operator=(const HostGroup &other);
};

#endif // HOSTGROUP_H
