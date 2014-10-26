/* Copyright 2014 Hallowyn and others.
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

#define DEFAULT_CANCEL_DELAY 900000
// 900" = 15'
#define DEFAULT_REMIND_FREQUENCY 3600000
// 3600" = 1h
#define DEFAULT_MIN_DELAY_BETWEEN_SEND 600000
// 600" = 10'
#define DEFAULT_GRACE_PERIOD_BEFORE_FIRST_SEND 30000
// 30"

class AlerterConfigData : public QSharedData {
public:
  ParamSet _params;
  QList<AlertRule> _rules;
  int _cancelDelay;
  int _minDelayBetweenSend;
  int _gracePeriodBeforeFirstSend;
  int _remindFrequency;
  QStringList _channelNames;
  AlerterConfigData() : _cancelDelay(DEFAULT_CANCEL_DELAY),
    _minDelayBetweenSend(DEFAULT_MIN_DELAY_BETWEEN_SEND),
    _gracePeriodBeforeFirstSend(DEFAULT_GRACE_PERIOD_BEFORE_FIRST_SEND),
    _remindFrequency(DEFAULT_REMIND_FREQUENCY) {
  }
  AlerterConfigData(PfNode root);
};

AlerterConfig::AlerterConfig() : d(new AlerterConfigData) {
}

AlerterConfig::AlerterConfig(const AlerterConfig &rhs) : d(rhs.d) {
}

AlerterConfig::AlerterConfig(PfNode root) : d(new AlerterConfigData(root)) {
}

AlerterConfigData::AlerterConfigData(PfNode root)
  : _cancelDelay(DEFAULT_CANCEL_DELAY),
    _minDelayBetweenSend(DEFAULT_MIN_DELAY_BETWEEN_SEND),
    _gracePeriodBeforeFirstSend(DEFAULT_GRACE_PERIOD_BEFORE_FIRST_SEND),
    _remindFrequency(DEFAULT_REMIND_FREQUENCY) {
  _channelNames << "mail" << "url" << "log" << "stop";
  ConfigUtils::loadParamSet(root, &_params, "param");
  foreach (PfNode rulenode, root.childrenByName("rule")) {
    //Log::debug() << "found alert rule section " << pattern << " " << stop;
    foreach (PfNode rulechannelnode, rulenode.children()) {
      if (rulechannelnode.name() == "match"
          || rulechannelnode.name() == "param") {
        // ignore
      } else {
        if (_channelNames.contains(rulechannelnode.name())) {
          AlertRule rule(rulenode, rulechannelnode, _params);
          _rules.append(rule);
          Log::debug() << "configured alert rule " << rulechannelnode.name()
                       << " " << rule.pattern() << " "
                       << rule.patternRegExp().pattern();
        } else {
          Log::warning() << "alert channel '" << rulechannelnode.name()
                         << "' unknown in alert rule";
        }
      }
    }
  }
  _cancelDelay =
      _params.valueAsInt("canceldelay", DEFAULT_CANCEL_DELAY/1000)*1000;
  if (_cancelDelay < 1000) // hard coded 1 second minmimum
    _cancelDelay = DEFAULT_CANCEL_DELAY;
  _minDelayBetweenSend =
      _params.valueAsInt("mindelaybetweensend",
                         DEFAULT_MIN_DELAY_BETWEEN_SEND/1000)*1000;
  if (_minDelayBetweenSend < 60000) // hard coded 1 minute minimum
    _minDelayBetweenSend = 60000;
  _gracePeriodBeforeFirstSend =
      _params.valueAsInt("graceperiodbeforefirstsend",
                         DEFAULT_GRACE_PERIOD_BEFORE_FIRST_SEND/1000)
      *1000;
  _remindFrequency =
      _params.valueAsInt("remindfrequency",
                         DEFAULT_REMIND_FREQUENCY/1000)*1000;
}

AlerterConfig &AlerterConfig::operator=(const AlerterConfig &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

AlerterConfig::~AlerterConfig() {
}

ParamSet AlerterConfig::params() const {
  return d ? d->_params : ParamSet();
}

int AlerterConfig::cancelDelay() const {
  return d ? d->_cancelDelay : DEFAULT_CANCEL_DELAY;
}

int AlerterConfig::minDelayBetweenSend() const {
  return d ? d->_minDelayBetweenSend : DEFAULT_MIN_DELAY_BETWEEN_SEND;
}

int AlerterConfig::gracePeriodBeforeFirstSend() const {
  return d ? d->_gracePeriodBeforeFirstSend
           : DEFAULT_GRACE_PERIOD_BEFORE_FIRST_SEND;
}

int AlerterConfig::remindFrequency() const {
  return d ? d->_remindFrequency : DEFAULT_REMIND_FREQUENCY;
}

QList<AlertRule> AlerterConfig::rules() const {
  return d ? d->_rules : QList<AlertRule>();
}

QStringList AlerterConfig::channelsNames() const {
  return d ? d->_channelNames : QStringList();
}
