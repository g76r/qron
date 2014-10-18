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
#include "confighistoryentry.h"

static QString _uiHeaderNames[] = {
  "History Entry Id", // 0
  "Config Id",
  "Activation Date",
  "Reason",
  "Actor"
};

class ConfigHistoryEntryData : public SharedUiItemData {
public:
  QString _id;
  SchedulerConfig _config;
  QDateTime _activationDate;
  QString _reason, _actor;
  ConfigHistoryEntryData(
      QString id, SchedulerConfig config, QDateTime activationDate,
      QString reason, QString actor)
    : _id(id), _config(config), _activationDate(activationDate),
      _reason(reason), _actor(actor) {
  }
  QVariant uiData(int section, int role) const;
  QVariant uiHeaderData(int section, int role) const;
  int uiDataCount() const;
  QString id() const { return _id; }
  void setId(QString id) { _id = id; }
  QString idQualifier() const { return "confighistoryentry"; }
};

ConfigHistoryEntry::ConfigHistoryEntry()
{
}

ConfigHistoryEntry::ConfigHistoryEntry(const ConfigHistoryEntry &other)
  : SharedUiItem(other) {
}

ConfigHistoryEntry::ConfigHistoryEntry(
    QString id, SchedulerConfig config, QDateTime activationDate,
    QString reason, QString actor)
  : SharedUiItem(new ConfigHistoryEntryData(id, config, activationDate,
                                            reason, actor)) {
}

// FIXME other cstr and accessors

QVariant ConfigHistoryEntryData::uiData(int section, int role) const {
  switch(role) {
  case Qt::DisplayRole:
    switch(section) {
    case 0:
      return _id;
    case 1:
      return _config.hash();
    case 2:
      return _activationDate.toString("yyyy-MM-dd hh:mm:ss,zzz");
    case 3:
      return _reason;
    case 4:
      return _actor;
    }
    break;
  default:
    ;
  }
  return QVariant();
}

QVariant ConfigHistoryEntryData::uiHeaderData(int section, int role) const {
  return role == Qt::DisplayRole && section >= 0
      && (unsigned)section < sizeof _uiHeaderNames
      ? _uiHeaderNames[section] : QVariant();
}

int ConfigHistoryEntryData::uiDataCount() const {
  return sizeof _uiHeaderNames / sizeof *_uiHeaderNames;
}

ConfigHistoryEntryData *ConfigHistoryEntry::che() {
  detach<ConfigHistoryEntryData>();
  return (ConfigHistoryEntryData*)constData();
}
