/* Copyright 2015 Hallowyn and others.
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
 * along with qron. If not, see <http://www.gnu.org/licenses/>.
 */
#include "alertsettings.h"
#include <QRegularExpression>
#include "config/configutils.h"
#include "log/log.h"

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Pattern",
  "Options",
  "Rise Delay",
  "Mayrise Delay",
  "Drop Delay" // 5
};

static QAtomicInt _sequence;

class AlertSettingsData : public SharedUiItemData {
public:
  QString _id, _pattern;
  QRegularExpression _patternRegexp;
  qint64 _riseDelay, _mayriseDelay, _dropDelay;
  // MAYDO add params

  AlertSettingsData()
    : _id(QString::number(_sequence.fetchAndAddOrdered(1))), _riseDelay(0),
      _mayriseDelay(0), _dropDelay(0) { }
  QString id() const { return _id; }
  QString idQualifier() const { return QStringLiteral("alertsettings"); }
  int uiSectionCount() const {
    return sizeof _uiHeaderNames / sizeof *_uiHeaderNames; }
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const {
    return role == Qt::DisplayRole && section >= 0
        && (unsigned)section < sizeof _uiHeaderNames
        ? _uiHeaderNames[section] : QVariant();
  }
};

AlertSettings::AlertSettings() {
}

AlertSettings::AlertSettings(const AlertSettings &other) : SharedUiItem(other) {
}

AlertSettings::AlertSettings(PfNode node) {
  AlertSettingsData *d = new AlertSettingsData;
  d->_pattern = node.attribute("match", "**");
  d->_patternRegexp = ConfigUtils::readDotHierarchicalFilter(d->_pattern);
  if (d->_pattern.isEmpty() || !d->_patternRegexp.isValid())
    Log::warning() << "unsupported alert settings match pattern '"
                   << d->_pattern << "': " << node.toString();
  d->_riseDelay = node.longAttribute(QStringLiteral("risedelay"), 0);
  d->_mayriseDelay = node.longAttribute(QStringLiteral("mayrisedelay"), 0);
  d->_dropDelay = node.longAttribute(QStringLiteral("dropdelay"), 0);
  setData(d);
}

PfNode AlertSettings::toPfNode() const {
  const AlertSettingsData *d = data();
  if (!d)
    return PfNode();
  PfNode node(QStringLiteral("settings"));
  node.setAttribute(QStringLiteral("pattern"), d->_pattern);
  if (d->_riseDelay > 0)
    node.setAttribute(QStringLiteral("risedelay"), d->_riseDelay);
  if (d->_mayriseDelay > 0)
    node.setAttribute(QStringLiteral("mayrisedelay"), d->_mayriseDelay);
  if (d->_dropDelay > 0)
    node.setAttribute(QStringLiteral("dropdelay"), d->_dropDelay);
  return node;
}

QString AlertSettings::pattern() const {
  const AlertSettingsData *d = data();
  return d ? d->_pattern : QString();
}

QRegularExpression AlertSettings::patternRegexp() const {
  const AlertSettingsData *d = data();
  return d ? d->_patternRegexp : QRegularExpression();
}

qint64 AlertSettings::riseDelay() const {
  const AlertSettingsData *d = data();
  return d ? d->_riseDelay : 0;
}

qint64 AlertSettings::mayriseDelay() const {
  const AlertSettingsData *d = data();
  return d ? d->_mayriseDelay : 0;
}

qint64 AlertSettings::dropDelay() const {
  const AlertSettingsData *d = data();
  return d ? d->_dropDelay : 0;
}

QVariant AlertSettingsData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      return _pattern;
    case 2: {
      QString s;
      if (_riseDelay > 0)
        s.append("risedelay=").append(QString::number(_riseDelay/1000));
      if (_mayriseDelay > 0)
        s.append("mayrisedelay=").append(QString::number(_mayriseDelay/1000));
      if (_dropDelay > 0)
        s.append("dropdelay=").append(QString::number(_dropDelay/1000));
      return s;
    }
    case 3:
      return _riseDelay > 0 ? QVariant(_riseDelay) : QVariant();
    case 4:
      return _mayriseDelay > 0 ? _mayriseDelay : QVariant();
    case 5:
      return _dropDelay > 0 ? _dropDelay : QVariant();
    }
    break;
  default:
    ;
  }
  return QVariant();
}
