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

QString ParamSet::value(const QString key) const {
  return d->value(key);
}

const QSet<QString> ParamSet::keys() const {
  QSet<QString> set = d->_params.keys().toSet();
  if (!d->_parent.isNull())
    set += d->_parent.keys(); // TODO parent should override child rather
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
