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
#ifndef CONFIGREPOSITORY_H
#define CONFIGREPOSITORY_H

#include <QObject>
#include <QStringList>
#include "config/schedulerconfig.h"
#include "sched/scheduler.h"
#include "confighistoryentry.h"

/** Configuration repository interface.
 * The whole object is thread-safe.
 * This implies that every implementation MUST be thread-safe.
 * @see LocalConfigRepository
 */
class LIBQRONSHARED_EXPORT ConfigRepository : public QObject {
  Q_OBJECT
  Scheduler *_scheduler;

public:
  ConfigRepository(QObject *parent, Scheduler *scheduler);
  virtual QStringList availlableConfigIds() = 0;
  /** Return id of active config according to repository, which is not
   * always the same currently than active config. */
  virtual QString activeConfigId() = 0;
  /** Syntaxic sugar for config(activeConfigId()) */
  SchedulerConfig activeConfig() { return config(activeConfigId()); }
  virtual SchedulerConfig config(QString id) = 0;
  /** Add a config to the repository if does not already exist, and return
   * its id. */
  virtual QString addConfig(SchedulerConfig config) = 0;
  /** Activate an already loaded config
   * @return fals if id not found */
  virtual bool activateConfig(QString id) = 0;
  /** Syntaxic sugar for activate(addConfig(config)) */
  QString addAndActivate(SchedulerConfig config) {
    QString id = addConfig(config);
    activateConfig(id);
    return id; }
  /** Syntaxic sugar for addConfig(parseConfig(source)) */
  QString addConfig(QIODevice *source, bool applyLogConfig) {
    return addConfig(parseConfig(source, applyLogConfig)); }
  /** Build a SchedulerConfig object from external format, without adding it
   * to the repository.
   * This method is thread-safe. */
  SchedulerConfig parseConfig(QIODevice *source, bool applyLogConfig);
  /** Remove a non-active config from the repository.
   * @return false if id not found or active */
  virtual bool removeConfig(QString id) = 0;

signals:
  void configActivated(QString id, SchedulerConfig config);
  void configAdded(QString id, SchedulerConfig config);
  void configRemoved(QString id);
  void historyReset(QList<ConfigHistoryEntry> history);
  void historyEntryAppended(ConfigHistoryEntry historyEntry);

private:
  Q_INVOKABLE SchedulerConfig parseConfig(PfNode source, bool applyLogConfig);
};

#endif // CONFIGREPOSITORY_H
