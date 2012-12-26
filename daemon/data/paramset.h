/* Copyright 2012 Hallowyn and others.
 * This file is part of qron, see <http://qron.hallowyn.com/>.
 * Qron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * Qron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef PARAMSET_H
#define PARAMSET_H

#include <QSharedData>
#include <QList>
#include <QStringList>
#include "log/log.h"

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
    return splitAndEvaluate(rawValue(key), separator); }
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
  QString toString() const;
};

QDebug operator<<(QDebug dbg, const ParamSet &params);

LogHelper operator <<(LogHelper lh, const ParamSet &params);

#endif // PARAMSET_H
