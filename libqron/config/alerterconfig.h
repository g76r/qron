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
#ifndef ALERTERCONFIG_H
#define ALERTERCONFIG_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include "pf/pfnode.h"
#include "util/paramset.h"
#include "alertrule.h"

class AlerterConfigData;

class LIBQRONSHARED_EXPORT AlerterConfig {
  QSharedDataPointer<AlerterConfigData> d;

public:
  AlerterConfig();
  AlerterConfig(const AlerterConfig &);
  AlerterConfig(PfNode root);
  AlerterConfig &operator=(const AlerterConfig &);
  ~AlerterConfig();
  /** Give access to alerts parameters. */
  ParamSet params() const;
  /** In ms. */
  int cancelDelay() const;
  /** In ms. */
  int minDelayBetweenSend() const;
  /** In ms. */
  int gracePeriodBeforeFirstSend() const;
  /** In ms. */
  int remindFrequency() const;
  QList<AlertRule> rules() const;
  /** Available channels names */
  QStringList channelsNames() const;
};

#endif // ALERTERCONFIG_H
