/* Copyright 2014-2015 Hallowyn and others.
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
#include "alerterconfig.h"
#include <QSharedData>
#include "configutils.h"

#define DEFAULT_RISE_DELAY 120000 /* 120" = 2' */
#define DEFAULT_MAYRISE_DELAY 60000 /* 60" = 1' */
#define DEFAULT_DROP_DELAY 900000 /* 900" = 15' */
#define DEFAULT_REMIND_PERIOD 3600000 /* 3600" = 1h */
#define DEFAULT_MIN_DELAY_BETWEEN_SEND 600000 /* 600" = 10' */
#define DEFAULT_DELAY_BEFORE_FIRST_SEND 30000 /* 30" */

static QString _uiHeaderNames[] = {
  "Id", // 0
  "Params",
  "Rise Delay",
  "Mayrise Delay",
  "Drop Delay",
  "Minimum Delay Between Send", // 5
  "Delay Before First Send",
  "Remind Period"
};

static QAtomicInt _sequence;

class AlerterConfigData : public SharedUiItemData {
public:
  QString _id;
  ParamSet _params;
  QList<AlertSubscription> _alertSubscriptions;
  QList<AlertSettings> _alertSettings;
  qint64 _riseDelay, _mayriseDelay, _dropDelay, _minDelayBetweenSend;
  qint64 _delayBeforeFirstSend, _remindPeriod;
  QStringList _channelNames;
  AlerterConfigData() : _id(QString::number(_sequence.fetchAndAddOrdered(1))),
    _riseDelay(DEFAULT_RISE_DELAY),
    _mayriseDelay(DEFAULT_MAYRISE_DELAY), _dropDelay(DEFAULT_DROP_DELAY),
    _minDelayBetweenSend(DEFAULT_MIN_DELAY_BETWEEN_SEND),
    _delayBeforeFirstSend(DEFAULT_DELAY_BEFORE_FIRST_SEND),
    _remindPeriod(DEFAULT_REMIND_PERIOD) {
  }
  AlerterConfigData(PfNode root);
  QString id() const { return _id; }
  QString idQualifier() const { return QStringLiteral("alerterconfig"); }
  int uiSectionCount() const {
    return sizeof _uiHeaderNames / sizeof *_uiHeaderNames; }
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const {
    return role == Qt::DisplayRole && section >= 0
        && (unsigned)section < sizeof _uiHeaderNames
        ? _uiHeaderNames[section] : QVariant();
  }
};

AlerterConfig::AlerterConfig(PfNode root)
  : SharedUiItem(new AlerterConfigData(root)) {
}

AlerterConfigData::AlerterConfigData(PfNode root)
  : _riseDelay(DEFAULT_RISE_DELAY), _mayriseDelay(DEFAULT_MAYRISE_DELAY),
    _dropDelay(DEFAULT_DROP_DELAY),
    _minDelayBetweenSend(DEFAULT_MIN_DELAY_BETWEEN_SEND),
    _delayBeforeFirstSend(DEFAULT_DELAY_BEFORE_FIRST_SEND),
    _remindPeriod(DEFAULT_REMIND_PERIOD) {
  _channelNames << "mail" << "url" << "log" << "stop";
  ConfigUtils::loadParamSet(root, &_params, "param");
  foreach (PfNode node, root.childrenByName("settings")) {
    AlertSettings settings(node);
    _alertSettings.append(settings);
    //Log::debug() << "configured alert settings " << settings.pattern() << " "
    //             << settings.patternRegexp().pattern() << " : "
    //             << settings.toPfNode().toString();
  }
  foreach (PfNode subscriptionnode, root.childrenByName("subscription")) {
    //Log::debug() << "found alert subscription section " << pattern << " " << stop;
    foreach (PfNode channelnode, subscriptionnode.children()) {
      if (channelnode.name() == "pattern"
          || channelnode.name() == "param") {
        // ignore
      } else {
        if (_channelNames.contains(channelnode.name())) {
          AlertSubscription sub(subscriptionnode, channelnode, _params);
          _alertSubscriptions.append(sub);
          //Log::debug() << "configured alert subscription " << channelnode.name()
          //             << " " << sub.pattern() << " "
          //             << sub.patternRegexp().pattern();
        } else {
          Log::warning() << "alert channel '" << channelnode.name()
                         << "' unknown in alert subscription";
        }
      }
    }
  }
  _riseDelay = root.longAttribute("risedelay", DEFAULT_RISE_DELAY/1000)*1000;
  if (_riseDelay < 1000) // hard coded 1 second minmimum
    _riseDelay = DEFAULT_RISE_DELAY;
  _mayriseDelay = root.longAttribute(
        "mayrisedelay", DEFAULT_MAYRISE_DELAY/1000)*1000;
  if (_mayriseDelay < 1000) // hard coded 1 second minmimum
    _mayriseDelay = DEFAULT_MAYRISE_DELAY;
  _dropDelay = root.longAttribute("dropdelay", DEFAULT_RISE_DELAY/1000)*1000;
  if (_dropDelay < 1000) // hard coded 1 second minmimum
    _dropDelay = DEFAULT_DROP_DELAY;
  _minDelayBetweenSend = root.longAttribute(
        "mindelaybetweensend", DEFAULT_MIN_DELAY_BETWEEN_SEND/1000)*1000;
  if (_minDelayBetweenSend < 60000) // hard coded 1 minute minimum
    _minDelayBetweenSend = 60000;
  _delayBeforeFirstSend = root.longAttribute(
        "delaybeforefirstsend", DEFAULT_DELAY_BEFORE_FIRST_SEND/1000)*1000;
  _remindPeriod = root.longAttribute(
        "remindperiod", DEFAULT_REMIND_PERIOD/1000)*1000;
}

ParamSet AlerterConfig::params() const {
  const AlerterConfigData *d = data();
  return d ? d->_params : ParamSet();
}

qint64 AlerterConfig::riseDelay() const {
  const AlerterConfigData *d = data();
  return d ? d->_riseDelay : DEFAULT_RISE_DELAY;
}

qint64 AlerterConfig::mayriseDelay() const {
  const AlerterConfigData *d = data();
  return d ? d->_mayriseDelay : DEFAULT_MAYRISE_DELAY;
}

qint64 AlerterConfig::dropDelay() const {
  const AlerterConfigData *d = data();
  return d ? d->_dropDelay : DEFAULT_DROP_DELAY;
}

qint64 AlerterConfig::minDelayBetweenSend() const {
  const AlerterConfigData *d = data();
  return d ? d->_minDelayBetweenSend : DEFAULT_MIN_DELAY_BETWEEN_SEND;
}

qint64 AlerterConfig::delayBeforeFirstSend() const {
  const AlerterConfigData *d = data();
  return d ? d->_delayBeforeFirstSend
           : DEFAULT_DELAY_BEFORE_FIRST_SEND;
}

qint64 AlerterConfig::remindPeriod() const {
  const AlerterConfigData *d = data();
  return d ? d->_remindPeriod : DEFAULT_REMIND_PERIOD;
}

QList<AlertSubscription> AlerterConfig::alertSubscriptions() const {
  const AlerterConfigData *d = data();
  return d ? d->_alertSubscriptions : QList<AlertSubscription>();
}

QList<AlertSettings> AlerterConfig::alertSettings() const {
  const AlerterConfigData *d = data();
  return d ? d->_alertSettings : QList<AlertSettings>();
}

QStringList AlerterConfig::channelsNames() const {
  const AlerterConfigData *d = data();
  return d ? d->_channelNames : QStringList();
}

PfNode AlerterConfig::toPfNode() const {
  const AlerterConfigData *d = data();
  if (!d)
    return PfNode();
  PfNode node("alerts");
  ConfigUtils::writeParamSet(&node, d->_params, "param");
  if (d->_riseDelay != DEFAULT_RISE_DELAY)
    node.setAttribute("risedelay", QString::number(d->_riseDelay/1000));
  if (d->_mayriseDelay != DEFAULT_MAYRISE_DELAY)
    node.setAttribute("mayrisedelay", QString::number(d->_mayriseDelay/1000));
  if (d->_dropDelay != DEFAULT_DROP_DELAY)
    node.setAttribute("dropdelay", QString::number(d->_dropDelay/1000));
  if (d->_minDelayBetweenSend != DEFAULT_MIN_DELAY_BETWEEN_SEND)
    node.setAttribute("mindelaybetweensend",
                      QString::number(d->_minDelayBetweenSend/1000));
  if (d->_delayBeforeFirstSend != DEFAULT_DELAY_BEFORE_FIRST_SEND)
    node.setAttribute("delaybeforefirstsend",
                      QString::number(d->_delayBeforeFirstSend/1000));
  if (d->_remindPeriod != DEFAULT_REMIND_PERIOD)
    node.setAttribute("remindperiod", QString::number(d->_remindPeriod/1000));
  foreach (const AlertSettings &settings, d->_alertSettings)
    node.appendChild(settings.toPfNode());
  foreach (const AlertSubscription &sub, d->_alertSubscriptions)
    node.appendChild(sub.toPfNode());
  return node;
}

QVariant AlerterConfigData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      return _params.toString(false, false);
    case 2:
      return _riseDelay > 0 ? _riseDelay/1000 : QVariant();
    case 3:
      return _mayriseDelay > 0 ? _mayriseDelay/1000 : QVariant();
    case 4:
      return _dropDelay > 0 ? _dropDelay/1000 : QVariant();
    case 5:
      return _minDelayBetweenSend > 0 ? _minDelayBetweenSend/1000 : QVariant();
    case 6:
      return _delayBeforeFirstSend > 0 ? _delayBeforeFirstSend/1000
                                       : QVariant();
    case 7:
      return _remindPeriod > 0 ? _remindPeriod/1000 : QVariant();
    }
    break;
  default:
    ;
  }
  return QVariant();
}
