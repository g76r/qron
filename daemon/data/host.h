#ifndef HOST_H
#define HOST_H

#include <QSharedDataPointer>

class HostData;

class Host {
  QSharedDataPointer<HostData> d;

public:
  Host();
  Host(const Host &other);
  ~Host();
  Host &operator=(const Host &);
};

#endif // HOST_H
