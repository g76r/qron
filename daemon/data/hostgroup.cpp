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
