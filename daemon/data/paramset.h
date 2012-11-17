#ifndef PARAMSET_H
#define PARAMSET_H

#include <QSharedData>
#include <QList>

class ParamSetData;

class ParamSet {
  friend class ParamSetData;
  QSharedDataPointer<ParamSetData> d;
  ParamSet(ParamSetData *data);
public:
  ParamSet();
  ParamSet(const ParamSet &other);
  ~ParamSet();
  const ParamSet parent() const;
  ParamSet parent();
  void setParent(ParamSet parent);
  void setValue(const QString key, const QString value);
  QString value(const QString key) const;
  const QSet<QString> keys() const;
  bool isNull() const;
};

QDebug operator<<(QDebug dbg, const ParamSet &params);

#endif // PARAMSET_H
