#ifndef PARAMSET_H
#define PARAMSET_H

#include <QSharedData>

class ParamSetData;

class ParamSet {
  QSharedDataPointer<ParamSetData> d;

public:
  ParamSet();
  ParamSet(const ParamSet &other);
  ~ParamSet();
  const ParamSet parent() const;
  ParamSet parent();
  void setParent(ParamSet parent);
  void setValue(const QString key, const QString value);
  QString value(const QString key) const;
  bool isNull() const;
};

#endif // PARAMSET_H
