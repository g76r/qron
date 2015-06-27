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
#ifndef ALERTSETTINGS_H
#define ALERTSETTINGS_H

#include "libqron_global.h"
#include "modelview/shareduiitem.h"
#include "pf/pfnode.h"

class AlertSettingsData;

/** Alert settings is the configuration object than enable overriding alerter
 * settings such as delays for specific alerts.
 * @see Alerter */
class LIBQRONSHARED_EXPORT AlertSettings : public SharedUiItem {
public:
  AlertSettings();
  AlertSettings(const AlertSettings &other);
  /** parses this form in Pf:
   * (settings
   *  (match task.**)
   *  (risedelay 30)
   *  (mayrisedelay 15)
   * )
   */
  explicit AlertSettings(PfNode node);
  AlertSettings &operator=(const AlertSettings &other) {
    SharedUiItem::operator=(other); return *this; }
  PfNode toPfNode() const;
  QString pattern() const;
  QRegularExpression patternRegexp() const;
  /** milliseconds, 0 when not set */
  qint64 riseDelay() const;
  /** milliseconds, 0 when not set */
  qint64 mayriseDelay() const;
  /** milliseconds, 0 when not set */
  qint64 dropDelay() const;
  /** milliseconds, 0 when not set */
  qint64 duplicateEmitDelay() const;

private:
  const AlertSettingsData *data() const {
    return (const AlertSettingsData*)SharedUiItem::data(); }
};

#endif // ALERTSETTINGS_H
