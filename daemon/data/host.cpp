#include "host.h"
#include <QSharedData>
#include <QString>
#include <QMap>
#include "pfnode.h"

class HostData : public QSharedData {
  friend class Host;
  QString _id, _label, _hostname;
  QMap<QString,qint64> _resources;
public:
};

Host::Host() : d(new HostData) {
}

Host::Host(const Host &other) : d(other.d) {
}

Host::Host(PfNode node) {
  HostData *hd = new HostData;
  hd->_id = node.attribute("id"); // LATER check uniqueness
  hd->_label = node.attribute("label", hd->_id);
  hd->_hostname = node.attribute("hostname", hd->_id);
  // TODO resources
  d = hd;
}

Host::~Host() {
}

Host &Host::operator=(const Host &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

QString Host::id() const {
  return d->_id;
}

QString Host::hostname() const {
  return d->_hostname;
}

bool Host::isNull() const {
  return d->_id.isNull();
}
