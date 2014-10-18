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
#ifndef CONFIGHISTORYENTRY_H
#define CONFIGHISTORYENTRY_H

#include "modelview/shareduiitem.h"
#include "config/schedulerconfig.h"

class ConfigHistoryEntryData;

class LIBQRONSHARED_EXPORT ConfigHistoryEntry : public SharedUiItem {
public:
  ConfigHistoryEntry();
  ConfigHistoryEntry(const ConfigHistoryEntry &other);
  ConfigHistoryEntry(
      QString id, SchedulerConfig config, QDateTime activationDate,
      QString reason, QString actor);
  ConfigHistoryEntry &operator=(const ConfigHistoryEntry &other) {
    SharedUiItem::operator=(other); return *this; }
  SchedulerConfig config() const;
  QDateTime activationDate() const;
  QString reason() const;
  QString actor() const;

private:
  ConfigHistoryEntryData *che();
  const ConfigHistoryEntryData *che() const {
    return (const ConfigHistoryEntryData*)constData(); }
};

#endif // CONFIGHISTORYENTRY_H
