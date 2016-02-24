/* Copyright 2012-2016 Hallowyn and others.
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
#ifndef WEBCONSOLE_H
#define WEBCONSOLE_H

#include "httpd/httphandler.h"
#include "httpd/templatinghttphandler.h"
#include "textview/htmltableview.h"
#include "textview/csvtableview.h"
#include "sched/scheduler.h"
#include "ui/hostsresourcesavailabilitymodel.h"
#include "ui/resourcesconsumptionmodel.h"
#include "ui/clustersmodel.h"
#include "util/paramsetmodel.h"
#include "ui/lastoccuredtexteventsmodel.h"
#include "textview/clockview.h"
#include "log/logmodel.h"
#include "ui/taskinstancesmodel.h"
#include "ui/tasksmodel.h"
#include "ui/schedulereventsmodel.h"
#include "ui/taskgroupsmodel.h"
#include "auth/inmemoryrulesauthorizer.h"
#include "auth/usersdatabase.h"
#include "httpd/graphvizimagehttphandler.h"
#include "ui/logfilesmodel.h"
#include <QSortFilterProxyModel>
#include "ui/htmlstepitemdelegate.h"
#include "configuploadhandler.h"
#include "configmgt/configrepository.h"
#include "ui/configsmodel.h"
#include "ui/htmlschedulerconfigitemdelegate.h"
#include "ui/confighistorymodel.h"
#include "thread/atomicvalue.h"
#include <QRegularExpression>
#include "modelview/shareduiitemslogmodel.h"
#include <QSortFilterProxyModel>

class QThread;

/** Central class for qron web console.
 * Mainly sets HTML templating and views up and dispatches request between views
 * and event handling. */
class WebConsole : public HttpHandler {
  friend class WebConsoleParamsProvider;
  Q_OBJECT
  Q_DISABLE_COPY(WebConsole)
  QThread *_thread;
  Scheduler *_scheduler;
  ConfigRepository *_configRepository;
  SharedUiItemsTableModel *_hostsModel;
  ClustersModel *_clustersModel;
  HostsResourcesAvailabilityModel *_freeResourcesModel, *_resourcesLwmModel;
  ResourcesConsumptionModel *_resourcesConsumptionModel;
  ParamSetModel *_globalParamsModel, *_globalSetenvModel, *_globalUnsetenvModel,
  *_alertParamsModel;
  SharedUiItemsTableModel *_raisableAlertsModel;
  QSortFilterProxyModel *_sortedRaisableAlertsModel, *_sortedRaisedAlertModel;
  LastOccuredTextEventsModel *_lastPostedNoticesModel; // TODO change to SUILogModel
  SharedUiItemsLogModel *_lastEmittedAlertsModel;
  SharedUiItemsTableModel *_alertSubscriptionsModel, *_alertSettingsModel,
  *_gridboardsModel;
  TaskInstancesModel *_taskInstancesHistoryModel, *_unfinishedTaskInstancetModel;
  TasksModel *_tasksModel;
  QSortFilterProxyModel *_mainTasksModel, *_subtasksModel;
  SchedulerEventsModel *_schedulerEventsModel;
  TaskGroupsModel *_taskGroupsModel;
  QSortFilterProxyModel *_sortedTaskGroupsModel;
  TextMatrixModel *_alertChannelsModel;
  LogFilesModel *_logConfigurationModel;
  SharedUiItemsTableModel *_calendarsModel;
  QSortFilterProxyModel *_sortedCalendarsModel;
  SharedUiItemsTableModel *_stepsModel;
  QSortFilterProxyModel *_sortedStepsModel;
  LogModel *_warningLogModel, *_infoLogModel, *_auditLogModel;
  ConfigsModel *_configsModel;
  ConfigHistoryModel *_configHistoryModel;
  HtmlTableView *_htmlHostsListView, *_htmlClustersListView,
  *_htmlFreeResourcesView, *_htmlResourcesLwmView,
  *_htmlResourcesConsumptionView, *_htmlGlobalParamsView,
  *_htmlGlobalSetenvView, *_htmlGlobalUnsetenvView, *_htmlAlertParamsView,
  *_htmlRaisableAlertsView, *_htmlRaisedAlertsView,
  *_htmlLastEmittedAlertsView, *_htmlAlertSubscriptionsView,
  *_htmlAlertSettingsView, *_htmlGridboardsView, *_htmlWarningLogView,
  *_htmlWarningLogView10, *_htmlInfoLogView, *_htmlAuditLogView,
  *_htmlTaskInstancesView, *_htmlTaskInstancesView20,
  *_htmlTasksScheduleView, *_htmlTasksConfigView, *_htmlTasksParamsView,
  *_htmlTasksListView,
  *_htmlTasksEventsView, *_htmlSchedulerEventsView,
  *_htmlLastPostedNoticesView20,
  *_htmlTaskGroupsView, *_htmlTaskGroupsEventsView,
  *_htmlAlertChannelsView, *_htmlTasksResourcesView, *_htmlTasksAlertsView,
  *_htmlLogFilesView, *_htmlCalendarsView, *_htmlStepsView,
  *_htmlConfigsView, *_htmlConfigHistoryView;
  HtmlSchedulerConfigItemDelegate *_htmlConfigsDelegate,
  *_htmlConfigHistoryDelegate;
  ClockView *_clockView;
  CsvTableView *_csvHostsListView,
  *_csvClustersListView, *_csvFreeResourcesView, *_csvResourcesLwmView,
  *_csvResourcesConsumptionView, *_csvGlobalParamsView,
  *_csvGlobalSetenvView, *_csvGlobalUnsetenvView,
  *_csvAlertParamsView, *_csvRaisableAlertsView, *_csvLastEmittedAlertsView,
  *_csvAlertSubscriptionsView, *_csvAlertSettingsView, *_csvGridboardsView,
  *_csvLogView, *_csvTaskInstancesView, *_csvTasksView,
  *_csvSchedulerEventsView, *_csvLastPostedNoticesView,
  *_csvTaskGroupsView, *_csvLogFilesView, *_csvCalendarsView, *_csvStepsView,
  *_csvConfigsView, *_csvConfigHistoryView;
  GraphvizImageHttpHandler *_tasksDeploymentDiagram, *_tasksTriggerDiagram;
  TemplatingHttpHandler *_wuiHandler;
  ConfigUploadHandler *_configUploadHandler;
  QString _configFilePath, _configRepoPath;
  InMemoryRulesAuthorizer *_authorizer;
  UsersDatabase *_usersDatabase;
  AtomicValue<QRegularExpression> _showAuditEvent, _hideAuditEvent,
  _showAuditUser, _hideAuditUser;
  AtomicValue<AlerterConfig> _alerterConfig;

public:
  WebConsole();
  ~WebConsole();
  bool acceptRequest(HttpRequest req);
  bool handleRequest(HttpRequest req, HttpResponse res,
                     ParamsProviderMerger *processingContext);
  void setScheduler(Scheduler *scheduler);
  void setConfigPaths(QString configFilePath, QString configRepoPath);
  void setConfigRepository(ConfigRepository *configRepository);
  void setAuthorizer(InMemoryRulesAuthorizer *authorizer);

public slots:
  void enableAccessControl(bool enabled);

private slots:
  void paramsChanged(ParamSet newParams, ParamSet oldParams, QString setId);
  void computeDiagrams(SchedulerConfig config);
  void alerterConfigChanged(AlerterConfig config);

private:
  static void copyFilteredFiles(QStringList paths, QIODevice *output,
                               QString pattern, bool useRegexp);
  static void copyFilteredFile(QString path, QIODevice *output,
                               QString pattern, bool useRegexp) {
    QStringList paths;
    paths.append(path);
    copyFilteredFiles(paths, output, pattern, useRegexp); }
};

#endif // WEBCONSOLE_H
