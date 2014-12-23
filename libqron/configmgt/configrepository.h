#ifndef CONFIGREPOSITORY_H
#define CONFIGREPOSITORY_H

#include <QObject>
#include <QStringList>
#include "config/schedulerconfig.h"
#include "sched/scheduler.h"
#include "confighistoryentry.h"

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
  QString addConfig(QIODevice *source) {
    return addConfig(parseConfig(source)); }
  /** Build a SchedulerConfig object from external format, without adding it
   * to the repository.
   * This method is thread-safe. */
  SchedulerConfig parseConfig(QIODevice *source);
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
  Q_INVOKABLE SchedulerConfig parseConfig(PfNode source);
};

#endif // CONFIGREPOSITORY_H
