/* Copyright 2012-2021 Hallowyn and others.
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
#include "log/logmodel.h"
#include "ui/taskinstancesmodel.h"
#include "ui/tasksmodel.h"
#include "ui/schedulereventsmodel.h"
#include "ui/taskgroupsmodel.h"
#include "auth/inmemoryrulesauthorizer.h"
#include "auth/usersdatabase.h"
#include "httpd/graphvizimagehttphandler.h"
#include <QSortFilterProxyModel>
#include "configuploadhandler.h"
#include "configmgt/configrepository.h"
#include "ui/configsmodel.h"
#include "htmlschedulerconfigitemdelegate.h"
#include "ui/confighistorymodel.h"
#include "thread/atomicvalue.h"
#include <QRegularExpression>
#include "modelview/shareduiitemslogmodel.h"
#include <QSortFilterProxyModel>
#include "io/readonlyresourcescache.h"

class QThread;

/** Central class for qron web console.
 * Mainly sets HTML templating and views up and dispatches request between views
 * and event handling. */
class WebConsole : public HttpHandler {
  friend class ServerStatsProvider;
  Q_OBJECT
  Q_DISABLE_COPY(WebConsole)
  QThread *_thread;
  Scheduler *_scheduler;
  ConfigRepository *_configRepository;
  SharedUiItemsTableModel *_hostsModel;
  QSortFilterProxyModel *_sortedHostsModel;
  ClustersModel *_clustersModel;
  QSortFilterProxyModel *_sortedClustersModel;
  HostsResourcesAvailabilityModel *_freeResourcesModel, *_resourcesLwmModel;
  ResourcesConsumptionModel *_resourcesConsumptionModel;
  ParamSetModel *_globalParamsModel, *_globalVarsModel,
  *_alertParamsModel;
  SharedUiItemsTableModel *_statefulAlertsModel;
  QSortFilterProxyModel *_sortedStatefulAlertsModel, *_sortedRaisedAlertModel;
  LastOccuredTextEventsModel *_lastPostedNoticesModel; // TODO change to SUILogModel
  SharedUiItemsLogModel *_lastEmittedAlertsModel;
  SharedUiItemsTableModel *_alertSubscriptionsModel, *_alertSettingsModel,
  *_gridboardsModel, *_logConfigurationModel;
  QSortFilterProxyModel *_sortedGridboardsModel;
  TaskInstancesModel *_taskInstancesHistoryModel, *_unfinishedTaskInstancesModel;
  QSortFilterProxyModel *_herdsHistoryModel;
  TasksModel *_tasksModel;
  QSortFilterProxyModel *_mainTasksModel;
  SchedulerEventsModel *_schedulerEventsModel;
  TaskGroupsModel *_taskGroupsModel;
  QSortFilterProxyModel *_sortedTaskGroupsModel;
  TextMatrixModel *_alertChannelsModel;
  SharedUiItemsTableModel *_calendarsModel;
  QSortFilterProxyModel *_sortedCalendarsModel;
  LogModel *_warningLogModel, *_infoLogModel, *_auditLogModel;
  ConfigsModel *_configsModel;
  ConfigHistoryModel *_configHistoryModel;
  HtmlTableView *_htmlHostsListView, *_htmlClustersListView,
  *_htmlFreeResourcesView, *_htmlResourcesLwmView,
  *_htmlResourcesConsumptionView, *_htmlGlobalParamsView,
  *_htmlGlobalVarsView, *_htmlAlertParamsView,
  *_htmlStatefulAlertsView, *_htmlRaisedAlertsView,
  *_htmlLastEmittedAlertsView, *_htmlAlertSubscriptionsView,
  *_htmlAlertSettingsView, *_htmlGridboardsView, *_htmlWarningLogView,
  *_htmlWarningLogView10, *_htmlInfoLogView, *_htmlAuditLogView,
  *_htmlTaskInstancesView, *_htmlUnfinishedTaskInstancesView, *_htmlHerdsView,
  *_htmlTasksScheduleView, *_htmlTasksConfigView, *_htmlTasksParamsView,
  *_htmlTasksListView,
  *_htmlTasksEventsView, *_htmlSchedulerEventsView,
  *_htmlLastPostedNoticesView20,
  *_htmlTaskGroupsView, *_htmlTaskGroupsEventsView,
  *_htmlAlertChannelsView, *_htmlTasksResourcesView, *_htmlTasksAlertsView,
  *_htmlLogFilesView, *_htmlCalendarsView,
  *_htmlConfigsView, *_htmlConfigHistoryView;
  HtmlSchedulerConfigItemDelegate *_htmlConfigsDelegate,
  *_htmlConfigHistoryDelegate;
  CsvTableView *_csvFreeResourcesView, *_csvResourcesLwmView,
  *_csvResourcesConsumptionView, *_csvGlobalParamsView,
  *_csvGlobalVarsView,
  *_csvAlertParamsView, *_csvStatefulAlertsView, *_csvLastEmittedAlertsView,
  *_csvGridboardsView, *_csvTaskInstancesView,
  *_csvSchedulerEventsView, *_csvLastPostedNoticesView,
  *_csvConfigsView, *_csvConfigHistoryView;
  GraphvizImageHttpHandler *_tasksDeploymentDiagram, *_tasksTriggerDiagram;
  TemplatingHttpHandler *_wuiHandler;
  ConfigUploadHandler *_configUploadHandler;
  QString _configFilePath, _configRepoPath;
  InMemoryRulesAuthorizer *_authorizer;
  AtomicValue<QRegularExpression> _showAuditEvent, _hideAuditEvent,
  _showAuditUser, _hideAuditUser;
  AtomicValue<AlerterConfig> _alerterConfig;
  ReadOnlyResourcesCache *_readOnlyResourcesCache;

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
  Scheduler *scheduler() const { return _scheduler; }
  ConfigRepository *configRepository() const { return _configRepository; }
  GraphvizImageHttpHandler *tasksDeploymentDiagram() const {
    return _tasksDeploymentDiagram; }
  GraphvizImageHttpHandler *tasksTriggerDiagram() const {
    return _tasksTriggerDiagram; }
  TemplatingHttpHandler *wuiHandler() const { return _wuiHandler; }
  ConfigUploadHandler *configUploadHandler() const {
    return _configUploadHandler; }
  QRegularExpression showAuditEvent() const { return _showAuditEvent; }
  QRegularExpression hideAuditEvent() const { return _hideAuditEvent; }
  QRegularExpression showAuditUser() const { return _showAuditUser; }
  QRegularExpression hideAuditUser() const { return _hideAuditUser; }
  AlerterConfig alerterConfig() const { return _alerterConfig; }
  CsvTableView *csvFreeResourcesView() const { return _csvFreeResourcesView; }
  CsvTableView *csvResourcesLwmView() const { return _csvResourcesLwmView; }
  CsvTableView *csvResourcesConsumptionView() const {
    return _csvResourcesConsumptionView; }
  CsvTableView *csvGlobalParamsView() const { return _csvGlobalParamsView; }
  CsvTableView *csvGlobalVarsView() const { return _csvGlobalVarsView; }
  CsvTableView *csvAlertParamsView() const { return _csvAlertParamsView; }
  CsvTableView *csvStatefulAlertsView() const { return _csvStatefulAlertsView; }
  CsvTableView *csvLastEmittedAlertsView() const {
    return _csvLastEmittedAlertsView; }
  CsvTableView *csvGridboardsView() const { return _csvGridboardsView; }
  CsvTableView *csvTaskInstancesView() const { return _csvTaskInstancesView; }
  CsvTableView *csvSchedulerEventsView() const {
    return _csvSchedulerEventsView; }
  CsvTableView *csvLastPostedNoticesView() const {
    return _csvLastPostedNoticesView; }
  CsvTableView *csvConfigsView() const { return _csvConfigsView; }
  CsvTableView *csvConfigHistoryView() const { return _csvConfigHistoryView; }
  HtmlTableView *htmlHostsListView() const { return _htmlHostsListView; }
  HtmlTableView *htmlClustersListView() const { return _htmlClustersListView; }
  HtmlTableView *htmlFreeResourcesView() const {
    return _htmlFreeResourcesView; }
  HtmlTableView *htmlResourcesLwmView() const { return _htmlResourcesLwmView; }
  HtmlTableView *htmlResourcesConsumptionView() const {
    return _htmlResourcesConsumptionView; }
  HtmlTableView *htmlGlobalParamsView() const { return _htmlGlobalParamsView; }
  HtmlTableView *htmlGlobalVarsView() const { return _htmlGlobalVarsView; }
  HtmlTableView *htmlAlertParamsView() const { return _htmlAlertParamsView; }
  HtmlTableView *htmlStatefulAlertsView() const {
    return _htmlStatefulAlertsView; }
  HtmlTableView *htmlRaisedAlertsView() const { return _htmlRaisedAlertsView; }
  HtmlTableView *htmlLastEmittedAlertsView() const {
    return _htmlLastEmittedAlertsView; }
  HtmlTableView *htmlAlertSubscriptionsView() const {
    return _htmlAlertSubscriptionsView; }
  HtmlTableView *htmlAlertSettingsView() const {
    return _htmlAlertSettingsView; }
  HtmlTableView *htmlGridboardsView() const { return _htmlGridboardsView; }
  HtmlTableView *htmlWarningLogView() const { return _htmlWarningLogView; }
  HtmlTableView *htmlWarningLogView10() const { return _htmlWarningLogView10; }
  HtmlTableView *htmlInfoLogView() const { return _htmlInfoLogView; }
  HtmlTableView *htmlAuditLogView() const { return _htmlAuditLogView; }
  HtmlTableView *htmlTaskInstancesView() const {
    return _htmlTaskInstancesView; }
  HtmlTableView *htmlTaskInstancesView20() const {
    return _htmlUnfinishedTaskInstancesView; }
  HtmlTableView *htmlTasksScheduleView() const {
    return _htmlTasksScheduleView; }
  HtmlTableView *htmlTasksConfigView() const { return _htmlTasksConfigView; }
  HtmlTableView *htmlTasksParamsView() const { return _htmlTasksParamsView; }
  HtmlTableView *htmlTasksListView() const { return _htmlTasksListView; }
  HtmlTableView *htmlTasksEventsView() const { return _htmlTasksEventsView; }
  HtmlTableView *htmlSchedulerEventsView() const {
    return _htmlSchedulerEventsView; }
  HtmlTableView *htmlLastPostedNoticesView20() const {
    return _htmlLastPostedNoticesView20; }
  HtmlTableView *htmlTaskGroupsView() const {
    return _htmlTaskGroupsView; }
  HtmlTableView *htmlTaskGroupsEventsView() const {
    return _htmlTaskGroupsEventsView; }
  HtmlTableView *htmlAlertChannelsView() const {
    return _htmlAlertChannelsView; }
  HtmlTableView *htmlTasksResourcesView() const {
    return _htmlTasksResourcesView; }
  HtmlTableView *htmlTasksAlertsView() const { return _htmlTasksAlertsView; }
  HtmlTableView *htmlLogFilesView() const { return _htmlLogFilesView; }
  HtmlTableView *htmlCalendarsView() const { return _htmlCalendarsView; }
  HtmlTableView *htmlConfigsView() const { return _htmlConfigsView; }
  HtmlTableView *htmlConfigHistoryView() const {
    return _htmlConfigHistoryView; }
  SharedUiItemList<> infoLogItems() const {
    return _infoLogModel ? _infoLogModel->items() : SharedUiItemList<>(); }
  SharedUiItemList<> warningLogItems() const {
    return _warningLogModel ? _warningLogModel->items() : SharedUiItemList<>();}
  SharedUiItemList<> auditLogItems() const {
    return _auditLogModel ? _auditLogModel->items() : SharedUiItemList<>(); }
  QString configFilePath() const { return _configFilePath; }
  QString configRepoPath() const { return _configRepoPath; }
  ReadOnlyResourcesCache *readOnlyResourcesCache() const {
    return _readOnlyResourcesCache; }

public slots:
  void enableAccessControl(bool enabled);

private slots:
  void paramsChanged(ParamSet newParams, ParamSet oldParams, QString setId);
  void computeDiagrams(SchedulerConfig config);
  void alerterConfigChanged(AlerterConfig config);
};

#endif // WEBCONSOLE_H
