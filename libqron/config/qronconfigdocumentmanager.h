/* Copyright 2015 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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
#ifndef QRONCONFIGDOCUMENTMANAGER_H
#define QRONCONFIGDOCUMENTMANAGER_H

#include "modelview/shareduiitemdocumentmanager.h"
#include "config/schedulerconfig.h"
#include "eventsubscription.h"

// FIXME doc
class LIBQRONSHARED_EXPORT QronConfigDocumentManager
    : public SharedUiItemDocumentManager {
  Q_OBJECT
  Q_DISABLE_COPY(QronConfigDocumentManager)
  SchedulerConfig _config;

public:
  explicit QronConfigDocumentManager(QObject *parent = 0);
  SchedulerConfig config() const { return _config; }
  void setConfig(SchedulerConfig newConfig);
  using SharedUiItemDocumentManager::itemById;
  SharedUiItem itemById(QString idQualifier, QString id) const override;
  using SharedUiItemDocumentManager::itemsByIdQualifier;
  SharedUiItemList<SharedUiItem> itemsByIdQualifier(
      QString idQualifier) const override;
  QHash<QString,Calendar> namedCalendars() const {
    return _config.namedCalendars(); }
  int tasksCount() const { return _config.tasks().count(); }
  int tasksGroupsCount() const { return _config.tasksGroups().count(); }
  int maxtotaltaskinstances() const { return _config.maxtotaltaskinstances(); }
  int maxqueuedrequests() const { return _config.maxqueuedrequests(); }
  Calendar calendarByName(QString name) const {
    return _config.namedCalendars().value(name); }
  ParamSet globalParams() const { return _config.globalParams(); }
  /** This method is threadsafe */
  bool taskExists(QString taskId) { return _config.tasks().contains(taskId); }
  /** This method is threadsafe */
  Task task(QString taskId) { return _config.tasks().value(taskId); }

signals:
  void logConfigurationChanged(QList<LogFile> logfiles);
  void globalParamsChanged(ParamSet globalParams);
  void globalSetenvChanged(ParamSet globalSetenv);
  void globalUnsetenvChanged(ParamSet globalUnsetenv);
  void accessControlConfigurationChanged(bool enabled);
  void globalEventSubscriptionsChanged(
      QList<EventSubscription> onstart, QList<EventSubscription> onsuccess,
      QList<EventSubscription> onfailure, QList<EventSubscription> onlog,
      QList<EventSubscription> onnotice,
      QList<EventSubscription> onschedulerstart,
      QList<EventSubscription> onconfigload);

protected:
  bool prepareChangeItem(
      SharedUiItemDocumentTransaction *transaction, SharedUiItem newItem,
      SharedUiItem oldItem, QString idQualifier, QString *errorString) override;
  void commitChangeItem(SharedUiItem newItem, SharedUiItem oldItem,
                        QString idQualifier) override;

private:
  template<class T>
  void inline emitSignalForItemTypeChanges(
      QHash<QString,T> newItems, QHash<QString,T> oldItems,
      QString idQualifier);
};

#endif // QRONCONFIGDOCUMENTMANAGER_H
