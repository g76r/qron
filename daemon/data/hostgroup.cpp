#include "hostgroup.h"
#include <QSharedData>
#include <QString>
#include "host.h"
#include <QList>

class HostGroupData : public QSharedData {
  QString _id, _label;
  QList<Host> _hosts;
public:
};

HostGroup::HostGroup() : d(new HostGroupData) {
}

HostGroup::HostGroup(const HostGroup &other) : d(other.d) {
}

HostGroup::~HostGroup() {
}

HostGroup &HostGroup::operator=(const HostGroup &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}
