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
  QList<ConfigHistoryEntry> _history;

public:
  ConfigRepository(QObject *parent, Scheduler *scheduler);
  virtual QStringList availlableConfigIds() = 0;
  /** Return id of current config according to repository, which is not
   * always the same currently than active config. */
  virtual QString currentConfigId() = 0;
  /** Syntaxic sugar for config(currentConfigId()) */
  SchedulerConfig currentConfig() { return config(currentConfigId()); }
  virtual SchedulerConfig config(QString id) = 0;
  /** Add a config to the repository if does not already exist, and return
   * its id. */
  virtual QString addConfig(SchedulerConfig config) = 0;
  virtual void setCurrent(QString id) = 0;
  /** Syntaxic sugar for setCurrent(addConfig(config)) */
  QString addCurrent(SchedulerConfig config) {
    QString id = addConfig(config);
    setCurrent(id);
    return id; }
  /** Syntaxic sugar for addConfig(parseConfig(source)) */
  QString addConfig(QIODevice *source) {
    return addConfig(parseConfig(source)); }
  /** Build a SchedulerConfig object from external format, without adding it
   * to the repository.
   * This method is thread-safe. */
  SchedulerConfig parseConfig(QIODevice *source);

signals:
  void currentConfigChanged(QString id, SchedulerConfig config);
  void configAdded(QString id, SchedulerConfig config);
  void configRemoved(QString id);
  void historyReset(QList<ConfigHistoryEntry> history);
  void historyAdded(ConfigHistoryEntry historyEntry);

private:
  Q_INVOKABLE SchedulerConfig parseConfig(PfNode source);
};

#endif // CONFIGREPOSITORY_H
