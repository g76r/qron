#ifndef PARAMSET_H
#define PARAMSET_H

#include <QSharedData>
#include <QList>
#include <QStringList>

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
  /** Return a value without interpreting parameters substitution.
    */
  QString rawValue(const QString key) const;
  /** Return a value after parameters substitution.
    * If the ParamSet does not hold a value for this key, search its
    * parents.
    */
  QString value(const QString key) const {
    return evaluate(rawValue(key)); }
  /** Return a value splitted into strings after parameters substitution.
    */
  QStringList valueAsStrings(const QString key,
                             const QString separator = " ") const {
    return splitAndEvaluate(rawValue(key)); }
  /** Return all keys for which the ParamSet or one of its parents hold a value.
    */
  const QSet<QString> keys() const;
  /** Perform parameters substitution.
    */
  QString evaluate(const QString rawValue) const;
  /** Split string and perform parameters substitution.
    */
  QStringList splitAndEvaluate(const QString rawValue,
                               const QString separator = " ") const;
  bool isNull() const;
};

QDebug operator<<(QDebug dbg, const ParamSet &params);

#endif // PARAMSET_H
