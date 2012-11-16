#include "host.h"
#include <QSharedData>
#include <QString>

class HostData : public QSharedData {
  QString _id, _label;

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
