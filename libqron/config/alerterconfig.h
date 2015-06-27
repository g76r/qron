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
#ifndef ALERTERCONFIG_H
#define ALERTERCONFIG_H

#include "libqron_global.h"
#include <QSharedDataPointer>
#include "pf/pfnode.h"
#include "util/paramset.h"
#include "alertsubscription.h"
#include "alertsettings.h"
#include "modelview/shareduiitem.h"

class AlerterConfigData;
class Gridboard;

/** Alerter config: main/root alert configuration object. */
class LIBQRONSHARED_EXPORT AlerterConfig : public SharedUiItem {

public:
  AlerterConfig() { }
  AlerterConfig(const AlerterConfig &other) : SharedUiItem(other) { }
  AlerterConfig(PfNode root);
  AlerterConfig &operator=(const AlerterConfig &other) {
    SharedUiItem::operator=(other); return *this; }
  /** Give access to alerts parameters. */
  ParamSet params() const;
  /** AlertConfig-level delay, in ms. */
  qint64 riseDelay() const;
  /** AlertConfig-level delay, in ms. */
  qint64 mayriseDelay() const;
  /** AlertConfig-level delay, in ms. */
  qint64 dropDelay() const;
  /** In ms. */
  qint64 duplicateEmitDelay() const;
  /** In ms. */
  qint64 minDelayBetweenSend() const;
  /** In ms. */
  qint64 delayBeforeFirstSend() const;
  /** In ms. */
  qint64 remindPeriod() const;
  QList<AlertSubscription> alertSubscriptions() const;
  QList<AlertSettings> alertSettings() const;
  /** Available channels names */
  QStringList channelsNames() const;
  PfNode toPfNode() const;
  QList<Gridboard> gridboards() const;

private:
  const AlerterConfigData *data() const {
    return (const AlerterConfigData *)SharedUiItem::data(); }
};

#endif // ALERTERCONFIG_H
