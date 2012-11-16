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

HostGroup::HostGroup() : data(new HostGroupData) {
}

HostGroup::HostGroup(const HostGroup &other) : data(other.data) {
}

HostGroup::~HostGroup() {
}

HostGroup &HostGroup::operator=(const HostGroup &other) {
  if (this != &other)
    data.operator=(other.data);
  return *this;
}
