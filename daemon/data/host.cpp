#include "host.h"
#include <QSharedData>
#include <QString>
#include <QMap>

class HostData : public QSharedData {
  QString _id, _label;
  QMap<QString,qint64> _resources;
public:
};

Host::Host() : d(new HostData) {
}

Host::Host(const Host &other) : d(other.d) {
}

Host::~Host() {
}

Host &Host::operator=(const Host &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}
