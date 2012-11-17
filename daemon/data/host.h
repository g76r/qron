#ifndef HOST_H
#define HOST_H

#include <QSharedDataPointer>

class HostData;
class PfNode;

class Host {
  QSharedDataPointer<HostData> d;

public:
  Host();
  Host(PfNode node);
  Host(const Host &other);
  ~Host();
  Host &operator=(const Host &);
  QString id() const;
  QString hostname() const;
  bool isNull() const;
};

#endif // HOST_H
