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
#include "paramset.h"
#include <QSharedData>
#include <QHash>
#include <QString>
#include <QtDebug>

class ParamSetData : public QSharedData {
  friend class ParamSet; // LATER cleanup the mix between friend and wrappers
  ParamSet _parent;
  QHash<QString,QString> _params;
  bool _isNull;
public:
  ParamSetData() : _parent(this), _isNull(true) { }
  ParamSetData(const ParamSetData &other) : QSharedData(),
    _parent(other._parent), _params(other._params), _isNull(other._isNull) { }
  QString value(const QString key) const {
    QString v = _params.value(key);
    return v.isNull() && !_parent.isNull() ? _parent.value(key) : v;
  }
  bool isNull() const { return _isNull; }
  const ParamSet parent() const { return _parent; }
  ParamSet parent() { return _parent; }
  void setParent(ParamSet parent) { _parent = parent; _isNull = false; }
  void setValue(const QString key, const QString value) {
    _params.insert(key, value); _isNull = false; }
};

ParamSet::ParamSet() : d(new ParamSetData()) {
}

ParamSet::ParamSet(const ParamSet &other) : d(other.d) {
}

ParamSet::ParamSet(ParamSetData *data) : d(data) {
}

ParamSet::~ParamSet() {
}

const ParamSet ParamSet::parent() const {
  return d->parent();
}

ParamSet ParamSet::parent() {
  return d->parent();
}

void ParamSet::setParent(ParamSet parent) {
  d->setParent(parent);
}

void ParamSet::setValue(const QString key, const QString value) {
  d->setValue(key, value);
}

QString ParamSet::rawValue(const QString key) const {
  QString value = d->value(key);
  //qDebug() << "    rawValue" << key << value << value.isNull() << isNull();
  if (value.isNull() && !isNull())
    return d->parent().rawValue(key);
  return value;
}

QString ParamSet::evaluate(const QString rawValue) const {
  // TODO parameters substitution
  return rawValue;
}

QStringList ParamSet::splitAndEvaluate(const QString rawValue,
                                       const QString separator) const {
  // TODO parameters substitution
  return rawValue.split(separator);
}

const QSet<QString> ParamSet::keys() const {
  QSet<QString> set = d->_params.keys().toSet();
  if (!d->_parent.isNull())
    set += d->_parent.keys();
  return set;
}

bool ParamSet::isNull() const {
  return d->isNull();
}

QDebug operator<<(QDebug dbg, const ParamSet &params) {
  dbg.nospace() << "{";
  if (!params.isNull())
    foreach(QString key, params.keys()) {
      dbg.space() << key << "=" << params.value(key) << ",";
    }
  dbg.nospace() << "}";
  return dbg.space();
}

LogHelper operator <<(LogHelper lh, const ParamSet &params) {
  lh << "{ ";
  if (!params.isNull())
    foreach(QString key, params.keys()) {
      lh << key << "=" << params.value(key) << " ";
    }
  return lh << "}";
}
