/* Copyright 2012-2013 Hallowyn and others.
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
#include "webconsole.h"
#include "util/ioutils.h"
#include <QFile>
#include <QThread>
#include "config/taskrequest.h"
#include "sched/qrond.h"
#include <QCoreApplication>
#include "util/htmlutils.h"

#define CONFIG_TABLES_MAXROWS 500
#define FLAGS_SET_MAXROWS 500
#define RAISED_ALERTS_MAXROWS 500

WebConsole::WebConsole() : _thread(new QThread), _scheduler(0),
  _hostsListModel(new HostsListModel(this)),
  _clustersListModel(new ClustersListModel(this)),
  _freeResourcesModel(new ResourcesAllocationModel(this)),
  _resourcesLwmModel(
    new ResourcesAllocationModel(this,
                                 ResourcesAllocationModel::LwmOverConfigured)),
  _resourcesConsumptionModel(new ResourcesConsumptionModel(this)),
  _globalParamsModel(new ParamSetModel(this)),
  _alertParamsModel(new ParamSetModel(this)),
  _raisedAlertsModel(new RaisedAlertsModel(this)),
  _lastEmitedAlertsModel(new LastEmitedAlertsModel(this, 500)),
  _lastPostedNoticesModel(new LastOccuredTextEventsModel(this, 200)),
  _lastFlagsChangesModel(new LastOccuredTextEventsModel(this, 200)),
  _alertRulesModel(new AlertRulesModel(this)),
  // memory cost: about 1.5 kB / request, e.g. 30 MB for 20000 requests
  // (this is an empirical measurement and thus includes model + csv view
  _taskRequestsHistoryModel(new TaskRequestsModel(this, 20000)),
  _unfinishedTaskRequestModel(new TaskRequestsModel(this, 1000, false)),
  _tasksModel(new TasksModel(this)),
  _schedulerEventsModel(new SchedulerEventsModel(this)),
  _flagsSetModel(new FlagsSetModel(this)),
  _taskGroupsModel(new TaskGroupsModel(this)),
  _alertChannelsModel(new AlertChannelsModel(this)),
  _htmlHostsListView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS)),
  _htmlClustersListView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS)),
  _htmlFreeResourcesView(
    new HtmlTableView(this, _htmlHostsListView->cachedRows())),
  _htmlResourcesLwmView(
    new HtmlTableView(this, _htmlHostsListView->cachedRows())),
  _htmlResourcesConsumptionView(
    new HtmlTableView(this, CONFIG_TABLES_MAXROWS)),
  _htmlGlobalParamsView(new HtmlTableView(this)),
  _htmlAlertParamsView(new HtmlTableView(this)),
  _htmlRaisedAlertsView(new HtmlTableView(this, RAISED_ALERTS_MAXROWS)),
  _htmlRaisedAlertsView10(new HtmlTableView(this, RAISED_ALERTS_MAXROWS, 10)),
  _htmlLastEmitedAlertsView(new HtmlTableView(
                              this, _lastEmitedAlertsModel->maxrows())),
  _htmlLastEmitedAlertsView10(new HtmlTableView(
                                this, _lastEmitedAlertsModel->maxrows(), 10)),
  _htmlAlertRulesView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS)),
  _htmlWarningLogView(new HtmlTableView(this, 500, 100)),
  _htmlWarningLogView10(new HtmlTableView(this, 100, 10)),
  _htmlInfoLogView(new HtmlTableView(this, 500, 100)),
  _htmlTaskRequestsView(new HtmlTableView(
                          this, _taskRequestsHistoryModel->maxrows(), 100)),
  _htmlTaskRequestsView20(new HtmlTableView(
                            this, _unfinishedTaskRequestModel->maxrows(), 20)),
  _htmlTasksScheduleView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS, 100)),
  _htmlTasksConfigView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS, 100)),
  _htmlTasksParamsView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS, 100)),
  _htmlTasksListView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS, 100)),
  _htmlTasksEventsView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS, 100)),
  _htmlSchedulerEventsView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS, 100)),
  _htmlLastPostedNoticesView20(new HtmlTableView(
                                 this, _lastPostedNoticesModel->maxrows(), 20)),
  _htmlLastFlagsChangesView20(new HtmlTableView(
                                this, _lastFlagsChangesModel->maxrows(), 20)),
  _htmlFlagsSetView20(new HtmlTableView(this, FLAGS_SET_MAXROWS, 20)),
  _htmlTaskGroupsView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS)),
  _htmlTaskGroupsEventsView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS)),
  _htmlAlertChannelsView(new HtmlTableView(this)),
  _htmlTasksResourcesView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS)),
  _htmlTasksAlertsView(new HtmlTableView(this, CONFIG_TABLES_MAXROWS, 100)),
  _clockView(new ClockView(this)),
  _csvHostsListView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvClustersListView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvFreeResourcesView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvResourcesLwmView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvResourcesConsumptionView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvGlobalParamsView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvAlertParamsView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvRaisedAlertsView(new CsvTableView(this, RAISED_ALERTS_MAXROWS)),
  _csvLastEmitedAlertsView(new CsvTableView(
                             this, _lastEmitedAlertsModel->maxrows())),
  _csvAlertRulesView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvLogView(new CsvTableView(this, _htmlInfoLogView->cachedRows())),
  _csvTaskRequestsView(new CsvTableView(
                         this, _taskRequestsHistoryModel->maxrows())),
  _csvTasksView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvSchedulerEventsView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _csvLastPostedNoticesView(new CsvTableView(
                              this, _lastPostedNoticesModel->maxrows())),
  _csvLastFlagsChangesView(new CsvTableView(
                             this, _lastFlagsChangesModel->maxrows())),
  _csvFlagsSetView(new CsvTableView(this, FLAGS_SET_MAXROWS)),
  _csvTaskGroupsView(new CsvTableView(this, CONFIG_TABLES_MAXROWS)),
  _tasksDeploymentDiagram(new GraphvizImageHttpHandler(this)),
  _tasksTriggerDiagram(new GraphvizImageHttpHandler(this)),
  _wuiHandler(new TemplatingHttpHandler(this, "/console", ":docroot/console")),
  _memoryInfoLogger(new MemoryLogger(0, Log::Info,
                                     _htmlInfoLogView->cachedRows())),
  _memoryWarningLogger(new MemoryLogger(0, Log::Warning,
                                        _htmlWarningLogView->cachedRows())),
  _title("Qron Web Console"), _navtitle("Qron Web Console"),
  _authorizer(new InMemoryRulesAuthorizer(this)), _usersDatabase(0),
  _ownUsersDatabase(false), _accessControlEnabled(false) {
  _thread->setObjectName("WebConsoleServer");
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  _htmlHostsListView->setModel(_hostsListModel);
  _htmlHostsListView->setTableClass("table table-condensed table-hover");
  _htmlHostsListView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlHostsListView->setEmptyPlaceholder("(no host)");
  _htmlClustersListView->setModel(_clustersListModel);
  _htmlClustersListView->setTableClass("table table-condensed table-hover");
  _htmlClustersListView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlClustersListView->setEmptyPlaceholder("(no cluster)");
  _htmlFreeResourcesView->setModel(_freeResourcesModel);
  _htmlFreeResourcesView->setTableClass("table table-condensed "
                                             "table-hover table-bordered");
  _htmlFreeResourcesView->setRowHeaders();
  _htmlFreeResourcesView->setEmptyPlaceholder("(no resource definition)");
  _htmlFreeResourcesView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlResourcesLwmView->setModel(_resourcesLwmModel);
  _htmlResourcesLwmView->setTableClass("table table-condensed "
                                             "table-hover table-bordered");
  _htmlResourcesLwmView->setRowHeaders();
  _htmlResourcesLwmView->setEmptyPlaceholder("(no resource definition)");
  _htmlResourcesLwmView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlResourcesConsumptionView->setModel(_resourcesConsumptionModel);
  _htmlResourcesConsumptionView
      ->setTableClass("table table-condensed table-hover table-bordered");
  _htmlResourcesConsumptionView->setRowHeaders();
  _htmlResourcesConsumptionView
      ->setEmptyPlaceholder("(no resource definition)");
  _htmlResourcesConsumptionView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlGlobalParamsView->setModel(_globalParamsModel);
  _htmlGlobalParamsView->setTableClass("table table-condensed table-hover");
  _htmlAlertParamsView->setModel(_alertParamsModel);
  _htmlAlertParamsView->setTableClass("table table-condensed table-hover");
  _raisedAlertsModel->setPrefix("<i class=\"icon-bell\"></i> ",
                                TextViews::HtmlPrefixRole);
  _htmlRaisedAlertsView->setModel(_raisedAlertsModel);
  _htmlRaisedAlertsView->setTableClass("table table-condensed table-hover");
  _htmlRaisedAlertsView->setEmptyPlaceholder("(no alert)");
  //_htmlRaisedAlertsView
  //    ->setEllipsePlaceholder("(alerts list too long to be displayed)");
  _htmlRaisedAlertsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlRaisedAlertsView10->setModel(_raisedAlertsModel);
  _htmlRaisedAlertsView10->setTableClass("table table-condensed table-hover");
  _htmlRaisedAlertsView10->setEmptyPlaceholder("(no alert)");
  //_htmlRaisedAlertsView10
  //    ->setEllipsePlaceholder("(see alerts page for more alerts)");
  _htmlRaisedAlertsView10->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _lastEmitedAlertsModel->setEventName("Alert");
  _lastEmitedAlertsModel->setPrefix("<i class=\"icon-bell\"></i> ", 0);
  _lastEmitedAlertsModel->setPrefix("<i class=\"icon-ok\"></i> ", 1);
  _lastEmitedAlertsModel->setPrefixRole(TextViews::HtmlPrefixRole);
  _htmlLastEmitedAlertsView->setModel(_lastEmitedAlertsModel);
  _htmlLastEmitedAlertsView->setTableClass("table table-condensed table-hover");
  _htmlLastEmitedAlertsView->setEmptyPlaceholder("(no alert)");
  //_htmlLastEmitedAlertsView
  //    ->setEllipsePlaceholder("(alerts list too long to be displayed)");
  _htmlLastEmitedAlertsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlLastEmitedAlertsView10->setModel(_lastEmitedAlertsModel);
  _htmlLastEmitedAlertsView10
      ->setTableClass("table table-condensed table-hover");
  _htmlLastEmitedAlertsView10->setEmptyPlaceholder("(no alert)");
  //_htmlLastEmitedAlertsView10
  //    ->setEllipsePlaceholder("(see alerts page for more alerts)");
  _htmlLastEmitedAlertsView10->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlAlertRulesView->setModel(_alertRulesModel);
  _htmlAlertRulesView->setTableClass("table table-condensed table-hover");
  _htmlAlertRulesView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlWarningLogView->setModel(_memoryWarningLogger->model());
  _htmlWarningLogView->setTableClass("table table-condensed table-hover");
  _htmlWarningLogView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlWarningLogView->setTrClassRole(LogModel::TrClassRole);
  //_htmlWarningLogView
  //    ->setEllipsePlaceholder("(download text log file for more entries)");
  _htmlWarningLogView->setEmptyPlaceholder("(empty log)");
  _htmlWarningLogView10->setModel(_memoryWarningLogger->model());
  _htmlWarningLogView10->setTableClass("table table-condensed table-hover");
  _htmlWarningLogView10->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlWarningLogView10->setTrClassRole(LogModel::TrClassRole);
  _htmlWarningLogView10->setEllipsePlaceholder("(see log page for more entries)");
  _htmlWarningLogView10->setEmptyPlaceholder("(empty log)");
  _htmlInfoLogView->setModel(_memoryInfoLogger->model());
  _htmlInfoLogView->setTableClass("table table-condensed table-hover");
  _htmlInfoLogView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlInfoLogView->setTrClassRole(LogModel::TrClassRole);
  //_htmlInfoLogView
  //    ->setEllipsePlaceholder("(download text log file for more entries)");
  _htmlWarningLogView->setEmptyPlaceholder("(empty log)");
  _htmlTaskRequestsView20->setModel(_unfinishedTaskRequestModel);
  _htmlTaskRequestsView20->setTableClass("table table-condensed table-hover");
  _htmlTaskRequestsView20->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTaskRequestsView20->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTaskRequestsView20->setTrClassRole(LogModel::TrClassRole);
  //_htmlTaskRequestsView20
  //    ->setEllipsePlaceholder("(see tasks page for more entries)");
  _htmlTaskRequestsView20->setEmptyPlaceholder("(no running or queued task)");
  _htmlTaskRequestsView->setModel(_taskRequestsHistoryModel);
  _htmlTaskRequestsView->setTableClass("table table-condensed table-hover");
  _htmlTaskRequestsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTaskRequestsView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTaskRequestsView->setTrClassRole(LogModel::TrClassRole);
  //_htmlTaskRequestsView
  //    ->setEllipsePlaceholder("(tasks list too long to be displayed)");
  _htmlTaskRequestsView->setEmptyPlaceholder("(no recent task)");
  _htmlTasksScheduleView->setModel(_tasksModel);
  _htmlTasksScheduleView->setTableClass("table table-condensed table-hover");
  _htmlTasksScheduleView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlTasksScheduleView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksScheduleView->setEmptyPlaceholder("(no task in configuration)");
  QList<int> cols;
  cols << 11 << 2 << 5 << 6 << 19 << 10 << 17 << 18;
  _htmlTasksScheduleView->setColumnIndexes(cols);
  _htmlTasksScheduleView->setRowAnchor("taskschedule.", 11);
  _htmlTasksConfigView->setModel(_tasksModel);
  _htmlTasksConfigView->setTableClass("table table-condensed table-hover");
  _htmlTasksConfigView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlTasksConfigView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksConfigView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 1 << 0 << 3 << 5 << 4 << 6 << 8 << 12;
  _htmlTasksConfigView->setColumnIndexes(cols);
  _htmlTasksConfigView->setRowAnchor("taskconfig.", 11);
  _htmlTasksParamsView->setModel(_tasksModel);
  _htmlTasksParamsView->setTableClass("table table-condensed table-hover");
  _htmlTasksParamsView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlTasksParamsView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksParamsView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 1 << 0 << 7 << 25 << 21 << 22;
  _htmlTasksParamsView->setColumnIndexes(cols);
  _htmlTasksParamsView->setRowAnchor("taskparams.", 11);
  _htmlTasksListView->setModel(_tasksModel);
  _htmlTasksListView->setTableClass("table table-condensed table-hover");
  _htmlTasksListView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlTasksListView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksListView->setEmptyPlaceholder("(no task in configuration)");
  _htmlTasksEventsView->setModel(_tasksModel);
  _htmlTasksEventsView->setTableClass("table table-condensed table-hover");
  _htmlTasksEventsView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlTasksEventsView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksEventsView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 11 << 6 << 14 << 15 << 16;
  _htmlTasksEventsView->setColumnIndexes(cols);
  _htmlSchedulerEventsView->setModel(_schedulerEventsModel);
  _htmlSchedulerEventsView->setTableClass("table table-condensed table-hover");
  _lastPostedNoticesModel->setEventName("Notice");
  _lastPostedNoticesModel->setPrefix("<i class=\"icon-comment\"></i> ");
  _lastPostedNoticesModel->setPrefixRole(TextViews::HtmlPrefixRole);
  _htmlLastPostedNoticesView20->setModel(_lastPostedNoticesModel);
  _htmlLastPostedNoticesView20
      ->setTableClass("table table-condensed table-hover");
  _htmlLastPostedNoticesView20->setEmptyPlaceholder("(no notice)");
  //_htmlLastPostedNoticesView20
  //    ->setEllipsePlaceholder("(older notices not displayed)");
  _htmlLastPostedNoticesView20->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _lastFlagsChangesModel->setEventName("Flag change");
  _lastFlagsChangesModel->setPrefix("<i class=\"icon-flag\"></i> ", 0);
  _lastFlagsChangesModel->setPrefix("<i class=\"icon-minus\"></i> ", 1);
  _lastFlagsChangesModel->setPrefixRole(TextViews::HtmlPrefixRole);
  _htmlLastFlagsChangesView20->setModel(_lastFlagsChangesModel);
  _htmlLastFlagsChangesView20
      ->setTableClass("table table-condensed table-hover");
  _htmlLastFlagsChangesView20->setEmptyPlaceholder("(no flags changes)");
  //_htmlLastFlagsChangesView20
  //    ->setEllipsePlaceholder("(older changes not displayed)");
  _htmlLastFlagsChangesView20->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _flagsSetModel->setPrefix("<i class=\"icon-flag\"></i> ",
                            TextViews::HtmlPrefixRole);
  _htmlFlagsSetView20->setModel(_flagsSetModel);
  _htmlFlagsSetView20->setTableClass("table table-condensed table-hover");
  _htmlFlagsSetView20->setEmptyPlaceholder("(no flags set)");
  //_htmlFlagsSetView20->setEllipsePlaceholder("(more flags not displayed)");
  _htmlFlagsSetView20->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTaskGroupsView->setModel(_taskGroupsModel);
  _htmlTaskGroupsView->setTableClass("table table-condensed table-hover");
  _htmlTaskGroupsView->setEmptyPlaceholder("(no task group)");
  _htmlTaskGroupsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  cols.clear();
  cols << 0 << 1 << 2 << 7 << 8;
  _htmlTaskGroupsView->setColumnIndexes(cols);
  _htmlTaskGroupsEventsView->setModel(_taskGroupsModel);
  _htmlTaskGroupsEventsView->setTableClass("table table-condensed table-hover");
  _htmlTaskGroupsEventsView->setEmptyPlaceholder("(no task group)");
  _htmlTaskGroupsEventsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  cols.clear();
  cols << 0 << 3 << 4 << 5;
  _htmlTaskGroupsEventsView->setColumnIndexes(cols);
  _htmlAlertChannelsView->setModel(_alertChannelsModel);
  _htmlAlertChannelsView->setTableClass("table table-condensed table-hover");
  _htmlAlertChannelsView->setRowHeaders();
  _htmlTasksResourcesView->setModel(_tasksModel);
  _htmlTasksResourcesView->setTableClass("table table-condensed table-hover");
  _htmlTasksResourcesView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTasksResourcesView->setEmptyPlaceholder("(no task)");
  cols.clear();
  cols << 11 << 17 << 8;
  _htmlTasksResourcesView->setColumnIndexes(cols);
  _htmlTasksAlertsView->setModel(_tasksModel);
  _htmlTasksAlertsView->setTableClass("table table-condensed table-hover");
  _htmlTasksAlertsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTasksAlertsView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksAlertsView->setEmptyPlaceholder("(no task)");
  cols.clear();
  cols << 11 << 6 << 23 << 26 << 24 << 27 << 12 << 16 << 18;
  _htmlTasksAlertsView->setColumnIndexes(cols);
  _clockView->setFormat("yyyy-MM-dd hh:mm:ss,zzz");
  _csvHostsListView->setModel(_hostsListModel);
  _csvHostsListView->setFieldQuote('"');
  _csvClustersListView->setModel(_clustersListModel);
  _csvClustersListView->setFieldQuote('"');
  _csvFreeResourcesView->setModel(_freeResourcesModel);
  _csvFreeResourcesView->setFieldQuote('"');
  _csvFreeResourcesView->setRowHeaders();
  _csvResourcesLwmView->setModel(_resourcesLwmModel);
  _csvResourcesLwmView->setFieldQuote('"');
  _csvResourcesLwmView->setRowHeaders();
  _csvResourcesConsumptionView->setModel(_resourcesConsumptionModel);
  _csvResourcesConsumptionView->setFieldQuote('"');
  _csvResourcesConsumptionView->setRowHeaders();
  _csvGlobalParamsView->setModel(_globalParamsModel);
  _csvGlobalParamsView->setFieldQuote('"');
  _csvAlertParamsView->setModel(_alertParamsModel);
  _csvAlertParamsView->setFieldQuote('"');
  _csvRaisedAlertsView->setModel(_raisedAlertsModel);
  _csvRaisedAlertsView->setFieldQuote('"');
  _csvLastEmitedAlertsView->setModel(_lastEmitedAlertsModel);
  _csvLastEmitedAlertsView->setFieldQuote('"');
  _csvLogView->setModel(_memoryInfoLogger->model());
  _csvLogView->setFieldQuote('"');
  _csvTaskRequestsView->setModel(_taskRequestsHistoryModel);
  _csvTaskRequestsView->setFieldQuote('"');
  _csvTasksView->setModel(_tasksModel);
  _csvTasksView->setFieldQuote('"');
  _csvSchedulerEventsView->setModel(_schedulerEventsModel);
  _csvSchedulerEventsView->setFieldQuote('"');
  _csvLastPostedNoticesView->setModel(_lastPostedNoticesModel);
  _csvLastPostedNoticesView->setFieldQuote('"');
  _csvLastFlagsChangesView->setModel(_lastFlagsChangesModel);
  _csvLastFlagsChangesView->setFieldQuote('"');
  _csvFlagsSetView->setModel(_flagsSetModel);
  _csvFlagsSetView->setFieldQuote('"');
  _csvTaskGroupsView->setModel(_taskGroupsModel);
  _csvTaskGroupsView->setFieldQuote('"');
  _wuiHandler->addFilter("\\.html$");
  _wuiHandler->addView("freeresources", _htmlFreeResourcesView);
  _wuiHandler->addView("resourceslwm", _htmlResourcesLwmView);
  _wuiHandler->addView("resourcesconsumption", _htmlResourcesConsumptionView);
  _wuiHandler->addView("hostslist", _htmlHostsListView);
  _wuiHandler->addView("clusterslist", _htmlClustersListView);
  _wuiHandler->addView("globalparams", _htmlGlobalParamsView);
  _wuiHandler->addView("alertparams", _htmlAlertParamsView);
  _wuiHandler->addView("raisedalerts", _htmlRaisedAlertsView);
  _wuiHandler->addView("raisedalerts10", _htmlRaisedAlertsView10);
  _wuiHandler->addView("lastemitedalerts", _htmlLastEmitedAlertsView);
  _wuiHandler->addView("lastemitedalerts10", _htmlLastEmitedAlertsView10);
  _wuiHandler->addView("now", _clockView);
  _wuiHandler->addView("alertrules", _htmlAlertRulesView);
  _wuiHandler->addView("warninglog10", _htmlWarningLogView10);
  _wuiHandler->addView("warninglog", _htmlWarningLogView);
  _wuiHandler->addView("infolog", _htmlInfoLogView);
  _wuiHandler->addView("taskrequests", _htmlTaskRequestsView);
  _wuiHandler->addView("taskrequests20", _htmlTaskRequestsView20);
  _wuiHandler->addView("tasksschedule", _htmlTasksScheduleView);
  _wuiHandler->addView("tasksconfig", _htmlTasksConfigView);
  _wuiHandler->addView("tasksparams", _htmlTasksParamsView);
  _wuiHandler->addView("tasksevents", _htmlTasksEventsView);
  _wuiHandler->addView("schedulerevents", _htmlSchedulerEventsView);
  _wuiHandler->addView("lastpostednotices20", _htmlLastPostedNoticesView20);
  _wuiHandler->addView("lastflagschanges20", _htmlLastFlagsChangesView20);
  _wuiHandler->addView("flagsset20", _htmlFlagsSetView20);
  _wuiHandler->addView("taskgroups", _htmlTaskGroupsView);
  _wuiHandler->addView("taskgroupsevents", _htmlTaskGroupsEventsView);
  _wuiHandler->addView("alertchannels", _htmlAlertChannelsView);
  _wuiHandler->addView("tasksresources", _htmlTasksResourcesView);
  _wuiHandler->addView("tasksalerts", _htmlTasksAlertsView);
  _memoryWarningLogger->model()
      ->setWarningIcon("<i class=\"icon-warning-sign\"></i> ");
  _memoryWarningLogger->model()
      ->setErrorIcon("<i class=\"icon-minus-sign\"></i> ");
  _memoryWarningLogger->model()->setWarningTrClass("warning");
  _memoryWarningLogger->model()->setErrorTrClass("error");
  _memoryInfoLogger->model()
      ->setWarningIcon("<i class=\"icon-warning-sign\"></i> ");
  _memoryInfoLogger->model()
      ->setErrorIcon("<i class=\"icon-minus-sign\"></i> ");
  _memoryInfoLogger->model()->setWarningTrClass("warning");
  _memoryInfoLogger->model()->setErrorTrClass("error");
  connect(this, SIGNAL(flagChange(QString,int)),
          _lastFlagsChangesModel, SLOT(eventOccured(QString,int)));
  connect(this, SIGNAL(alertEmited(QString,int)),
          _lastEmitedAlertsModel, SLOT(eventOccured(QString,int)));
  moveToThread(_thread);
  _memoryInfoLogger->moveToThread(_thread); // TODO won't be deleted
  _memoryWarningLogger->moveToThread(_thread);
  _authorizer->allow("", "/console/(css|jsp|js)/.*") // anyone for static rsrc
      .allow("operate", "/(rest|console)/do") // operate for operation
      .deny("", "/(rest|console)/do") // nobody else
      .allow("read"); // read for everything else
}

WebConsole::~WebConsole() {
  _memoryInfoLogger->moveToThread(QCoreApplication::instance()->thread());
  _memoryWarningLogger->moveToThread(QCoreApplication::instance()->thread());
}

bool WebConsole::acceptRequest(HttpRequest req) {
  Q_UNUSED(req)
  return true;
}

class WebConsoleParamsProvider : public ParamsProvider {
  WebConsole *_console;
  QString _message;
  QHash<QString,QString> _values;
  HttpRequest _req;
  HttpRequestContext _ctxt;
public:
  WebConsoleParamsProvider(WebConsole *console, HttpRequest req,
                           HttpResponse res, HttpRequestContext ctxt)
    : _console(console), _req(req), _ctxt(ctxt) {
    QString message = req.base64Cookie("message");
    //qDebug() << "message cookie:" << message;
    if (!message.isEmpty()) {
      char alertType = 'I';
      QString title;
      QString alertClass;
      if (message.size() > 1 && message.at(1) == ':') {
        alertType = message.at(0).toAscii();
        message = message.mid(2);
      }
      switch (alertType) {
      case 'S':
        title = "Success:";
        alertClass = "alert-success";
        break;
      case 'I':
        title = "Info:";
        alertClass = "alert-info";
        break;
      case 'E':
        title = "Error!";
        alertClass = "alert-error";
        break;
      case 'W':
        title = "Warning!";
        break;
      default:
        ;
      }
      _message =  "<div class=\"alert alert-block "+alertClass+"\">"
          "<button type=\"button\" class=\"close\" data-dismiss=\"alert\">"
          "&times;</button><h4>"+title+"</h4> "+message+"</div>";
    } else
      _message = "";
    res.clearCookie("message", "/");
  }
  QVariant paramValue(const QString key, const QVariant defaultValue) const {
    if (_values.contains(key))
      return _values.value(key);
    if (!_console->_scheduler) // should never happen
      return defaultValue;
    if (key.startsWith('%'))
      return _console->_scheduler->globalParams().evaluate(key);
    if (key == "title") // TODO remove, needs support for ${webconsole.title:-Qron Web Console}
      return _console->_title;
    if (key == "navtitle") // TODO remove
      return _console->_navtitle;
    if (key == "cssoverload") // TODO remove
      return _console->_cssoverload;
    if (key == "message")
      return _message;
    if (key == "startdate")
      return _console->_scheduler->startdate().toString("yyyy-MM-dd hh:mm:ss");
    if (key == "configdate")
      return _console->_scheduler->configdate().toString("yyyy-MM-dd hh:mm:ss");
    if (key == "execcount")
      return QString::number(_console->_scheduler->execCount());
    if (key == "taskscount")
      return QString::number(_console->_scheduler->tasksCount());
    if (key == "tasksgroupscount")
      return QString::number(_console->_scheduler->tasksGroupsCount());
    if (key == "maxtotaltaskinstances")
      return QString::number(_console->_scheduler->maxtotaltaskinstances());
    if (key == "maxqueuedrequests")
      return QString::number(_console->_scheduler->maxqueuedrequests());
    QString v(_req.base64Cookie(key));
    return v.isNull() ? _ctxt.paramValue(key, defaultValue) : v;
  }
  void setValue(QString key, QString value) {
    _values.insert(key, value);
  }
};

bool WebConsole::handleRequest(HttpRequest req, HttpResponse res,
                               HttpRequestContext ctxt) {
  Q_UNUSED(ctxt)
  QString path = req.url().path();
  while (path.size() && path.at(path.size()-1) == '/')
    path.chop(1);
  if (path.isEmpty()) {
    res.redirect("console/index.html");
    return true;
  }
  if (_accessControlEnabled
      && !_authorizer->authorize(ctxt.paramValue("userid").toString(), path)) {
    res.setStatus(403);
    QUrl url(req.url());
    url.setPath("/console/adhoc.html");
    url.setQueryItems(QList<QPair<QString,QString> >());
    req.overrideUrl(url);
    WebConsoleParamsProvider params(this, req, res, ctxt);
    params.setValue("content", "<h2>Permission denied</h2>");
    _wuiHandler->handleRequest(req, res, HttpRequestContext(&params));
    return true;
  }
  //Log::fatal() << "hit: " << req.url().toString();
  if (path == "/console/do" || path == "/rest/do" ) {
    QString event = req.param("event");
    QString fqtn = req.param("fqtn");
    QString alert = req.param("alert");
    quint64 id = req.param("id").toULongLong();
    QString referer = req.header("Referer");
    QString redirect = req.base64Cookie("redirect", referer);
    QString message;
    if (_scheduler) {
      if (event == "requestTask") {
        // 192.168.79.76:8086/console/do?event=requestTask&fqtn=appli.batch.batch1
        ParamSet params(req.paramsAsParamSet());
        // TODO handle null values rather than replacing empty with nulls
        foreach (QString key, params.keys())
          if (params.value(key).isEmpty())
            params.removeValue(key);
        TaskRequest request = _scheduler->syncRequestTask(fqtn, params);
        if (!request.isNull())
          message = "S:Task '"+fqtn+"' submitted for execution with id "
              +QString::number(request.id())+".";
        else
          message = "E:Execution request of task '"+fqtn
              +"' failed (see logs for more information).";
      } else if (event == "cancelRequest") {
        TaskRequest request = _scheduler->cancelRequest(id);
        if (!request.isNull())
          message = "S:Task request "+QString::number(id)+" canceled.";
        else
          message = "E:Cannot cancel request "+QString::number(id)+".";
      } else if (event == "abortTask") {
        TaskRequest request = _scheduler->abortTask(id);
        if (!request.isNull())
          message = "S:Task "+QString::number(id)+" aborted.";
        else
          message = "E:Cannot abort task "+QString::number(id)+".";
      } else if (event == "enableTask") {
        bool enable = req.param("enable") == "true";
        if (_scheduler->enableTask(fqtn, enable))
          message = "S:Task '"+fqtn+"' "+(enable?"enabled":"disabled")+".";
        else
          message = "E:Task '"+fqtn+"' not found.";
      } else if (event == "cancelAlert") {
        if (req.param("immediately") == "true")
          _scheduler->alerter()->cancelAlertImmediately(alert);
        else
          _scheduler->alerter()->cancelAlert(alert);
        message = "S:Canceled alert '"+alert+"'.";
      } else if (event == "raiseAlert") {
        _scheduler->alerter()->raiseAlert(alert);
        message = "S:Raised alert '"+alert+"'.";
      } else if (event == "emitAlert") {
        _scheduler->alerter()->emitAlert(alert);
        message = "S:Emitted alert '"+alert+"'.";
      } else if (event=="enableAllTasks") {
        bool enable = req.param("enable") == "true";
        _scheduler->enableAllTasks(enable);
        message = QString("S:Asked for ")+(enable?"enabling":"disabling")
            +" all tasks at once.";
      } else if (event=="reloadConfig") {
        bool ok = Qrond::instance()->reload();
        message = ok ? "S:Configuration reloaded."
                     : "E:Cannot reload configuration.";
      } else
        message = "E:Internal error: unknown event '"+event+"'.";
    } else
      message = "E:Scheduler is not available.";
    if (!redirect.isEmpty()) {
      res.setBase64SessionCookie("message", message, "/");
      res.clearCookie("redirect", "/");
      res.redirect(redirect);
    } else {
      if (message.startsWith("E:") || message.startsWith("W:"))
        res.setStatus(500);
      res.setContentType("text/plain;charset=UTF-8");
      message.append("\n");
      res.output()->write(message.toUtf8());
    }
    return true;
  }
  if (path == "/console/confirm") {
    QString event = req.param("event");
    QString fqtn = req.param("fqtn");
    QString id = req.param("id");
    QString referer = req.header("Referer", "index.html");
    QString message;
    if (_scheduler) {
      if (event == "abortTask") {
        message = "abort task "+id;
      } else if (event == "cancelRequest") {
        message = "cancel request "+id;
      } else if (event == "enableAllTasks") {
        message = QString(req.param("enable") == "true" ? "enable" : "disable")
            + " all tasks";
      } else if (event == "enableTask") {
        message = QString(req.param("enable") == "true" ? "enable" : "disable")
            + " task '"+fqtn+"'";
      } else if (event == "reloadConfig") {
        message = "reload configuration";
      } else if (event == "requestTask") {
        message = "request task '"+fqtn+"' execution";
      } else {
        message = event;
      }
      message = "<div class=\"alert alert-block\">"
          "<h4 class=\"text-center\">Are you sure you want to "+message
          +" ?</h4><p><p class=\"text-center\"><a class=\"btn btn-danger\" "
          "href=\"do?"+req.url().toString().remove(QRegExp("^[^\\?]*\\?"))
          +"\">Yes, sure</a> <a class=\"btn\" href=\""+referer+"\">Cancel</a>"
          "</div>";
      res.setBase64SessionCookie("redirect", referer, "/");
      QUrl url(req.url());
      url.setPath("/console/adhoc.html");
      url.setQueryItems(QList<QPair<QString,QString> >());
      req.overrideUrl(url);
      WebConsoleParamsProvider params(this, req, res, ctxt);
      params.setValue("content", message);
      _wuiHandler->handleRequest(req, res, HttpRequestContext(&params));
      return true;
    } else {
      res.setBase64SessionCookie("message", "E:Scheduler is not available.",
                                 "/");
      res.redirect(referer);
    }
  }
  if (path == "/console/requestform") {
    QString fqtn = req.param("fqtn");
    QString referer = req.header("Referer", "index.html");
    QString redirect = req.base64Cookie("redirect", referer);
    if (_scheduler) {
      Task task(_scheduler->task(fqtn));
      if (!task.isNull()) {
        QUrl url(req.url());
        url.setPath("/console/adhoc.html");
        req.overrideUrl(url);
        WebConsoleParamsProvider params(this, req, res, ctxt);
        QString form = "<div class=\"alert alert-block\">\n"
            "<h4 class=\"text-center\">About to start task "+fqtn+"</h4>\n";
        if (task.label() != task.id())
          form +="<h4 class=\"text-center\">("+task.label()+")</h4>\n";
        form += "<p>\n";
        if (task.requestFormFields().size())
          form += "<p class=\"text-center\">Task parameters can be defined in "
              "the following form:";
        form += "<p><form class=\"form-horizontal\" action=\"do\">";
        foreach (RequestFormField rff, task.requestFormFields())
          form.append(rff.toHtmlFormFragment("input-xxlarge"));
        /*form += "<p><p class=\"text-center\"><a class=\"btn btn-danger\" "
            "href=\"do?"+req.url().toString().remove(QRegExp("^[^\\?]*\\?"))
            +"\">Request task execution</a> <a class=\"btn\" href=\""
            +referer+"\">Cancel</a>\n"*/
        form += "<input type=\"hidden\" name=\"fqtn\" value=\""+fqtn+"\">\n"
            "<input type=\"hidden\" name=\"event\" value=\"requestTask\">\n"
            "<p class=\"text-center\">"
            //"<div class=\"control-group\"><div class=\"controls\">"
            "<button type=\"submit\" class=\"btn btn-danger\">"
            "Request task execution</button>\n"
            "<a class=\"btn\" href=\""+referer+"\">Cancel</a>\n"
            //"</div></div>\n"
            "</form>\n"
            "</div>\n";
        // <button type="submit" class="btn">Sign in</button>
        params.setValue("content", form);
        res.setBase64SessionCookie("redirect", redirect, "/");
        _wuiHandler->handleRequest(req, res, HttpRequestContext(&params));
        return true;
      } else {
        res.setBase64SessionCookie("message", "E:Task '"+fqtn+"' not found.",
                                   "/");
        res.redirect(referer);
      }
    } else {
      res.setBase64SessionCookie("message", "E:Scheduler is not available.",
                                 "/");
      res.redirect(referer);
    }
  }
  if (path == "/console/taskdoc.html") {
    QString fqtn = req.param("fqtn");
    QString referer = req.header("Referer", "index.html");
    if (_scheduler) {
      Task task(_scheduler->task(fqtn));
      if (!task.isNull()) {
        WebConsoleParamsProvider params(this, req, res, ctxt);
        int instancesCount = task.instancesCount();
        params.setValue("description",
                        "<tr><th>Fully qualified task name (fqtn)</th><td>"+fqtn
                        +"</td></tr>"
                        "<tr><th>Task id and label</th><td>"+task.id()
                        +((task.label()!=task.id())
                          ? " ("+HtmlUtils::htmlEncode(task.label())
                            +")</td></tr>" : "")
                        +"<tr><th>Task group id and label</th><td>"
                        +task.taskGroup().id()
                        +((task.taskGroup().label()!=task.taskGroup().id())
                          ? " ("+HtmlUtils::htmlEncode(task.taskGroup().label())
                            +")</td></tr>" : "")
                        +"<tr><th>Additional information</th><td>"
                        +HtmlUtils::htmlEncode(task.info())+"</td></tr>"
                        "<tr><th>Triggers (scheduling)</th><td>"
                        +task.triggersAsString()+"</td></tr>"
                        "<tr><th>Minimum expected duration (seconds)</th><td>"
                        +TasksModel::taskMinExpectedDuration(task)+"</td></tr>"
                        "<tr><th>Maximum expected duration (seconds)</th><td>"
                        +TasksModel::taskMaxExpectedDuration(task)+"</td></tr>"
                        "<tr><th>Maximum duration before abort "
                        "(seconds)</th><td>"
                        +TasksModel::taskMaxDurationBeforeAbort(task)
                        +"</td></tr>");
        params.setValue("status",
                        "<tr><th>Enabled</th><td>"
                        +QString(task.enabled()
                                 ? "true"
                                 : "<i class=\"icon-ban-circle\"></i> false")
                        +"</td></tr><tr><th>Last execution status</th><td>"
                        +HtmlUtils::htmlEncode(
                          TasksModel::taskLastExecStatus(task))+"</td></tr>"
                        "<tr><th>Last execution duration (seconds)</th><td>"
                        +HtmlUtils::htmlEncode(
                          TasksModel::taskLastExecDuration(task))+"</td></tr>"
                        "<tr><th>Next execution</th><td>"
                        +task.nextScheduledExecution()
                        .toString("yyyy-MM-dd hh:mm:ss,zzz")+"</td></tr>"
                        "<tr><th>Currently running instances</th><td>"
                        +(instancesCount ? "<i class=\"icon-play\"></i> " : "")
                        +QString::number(instancesCount)+" / "
                        +QString::number(task.maxInstances())+"</td></tr>");
        params.setValue("params",
                        "<tr><th>Execution mean</th><td>"+task.mean()
                        +"</td></tr>"
                        "<tr><th>Execution target (host or cluster)</th><td>"
                        +task.target()+"</td></tr>"
                        "<tr><th>Command</th><td>"
                        +HtmlUtils::htmlEncode(task.command())+"</td></tr>"
                        "<tr><th>Configuration parameters</th><td>"
                        +HtmlUtils::htmlEncode(task.params().toString(false))
                        +"</td></tr>"
                        "<tr><th>Request-time overridable parameters</th><td>"
                        +task.requestFormFieldsAsHtmlDescription()+"</td></tr>"
                        "<tr><th>System environment variables set (setenv)</th>"
                        "<td>"+TasksModel::taskSetenv(task)+"</td></tr>"
                        "<tr><th>System environment variables unset (unsetenv)"
                        "</th><td>"+TasksModel::taskUnsetenv(task)+"</td></tr>"
                        "<tr><th>Consumed resources</th><td>"
                        +task.resourcesAsString()+"</td></tr>"
                        "<tr><th>Maximum instances count at a time</th><td>"
                        +QString::number(task.maxInstances())+"</td></tr>"
                        "<tr><th>Onstart events</th><td>"
                        +Event::toStringList(task.onstartEvents()).join(" ")
                        +"</td></tr>"
                        "<tr><th>Onsuccess events</th><td>"
                        +Event::toStringList(task.onsuccessEvents()).join(" ")
                        +"</td></tr>"
                        "<tr><th>Onfailure events</th><td>"
                        +Event::toStringList(task.onfailureEvents()).join(" ")
                        +"</td></tr>");
        params.setValue("fqtn", fqtn);
        params.setValue("customactions", task.params()
                        .evaluate(_customaction_taskdetail, &task));
        _wuiHandler->handleRequest(req, res, HttpRequestContext(&params));
        return true;
      } else {
        res.setBase64SessionCookie("message", "E:Task '"+fqtn+"' not found.",
                                   "/");
        res.redirect(referer);
      }
    } else {
      res.setBase64SessionCookie("message", "E:Scheduler is not available.",
                                 "/");
      res.redirect(referer);
    }
  }
  if (path.startsWith("/console")) {
    QList<QPair<QString,QString> > queryItems(req.url().queryItems());
    if (queryItems.size()) {
      // if there are query parameters in url, transform them into cookies
      // LATER this mechanism should be generic/framework (in libqtssu)
      QListIterator<QPair<QString,QString> > it(queryItems);
      QString anchor;
      while (it.hasNext()) {
        const QPair<QString,QString> &p(it.next());
        if (p.first == "anchor")
          anchor = p.second;
        else
          res.setBase64SessionCookie(p.first, p.second, "/");
      }
      QString s = req.url().path();
      int i = s.lastIndexOf('/');
      if (i != -1)
        s = s.mid(i+1);
      if (!anchor.isEmpty())
        s.append('#').append(anchor);
      res.redirect(s);
    } else {
      WebConsoleParamsProvider params(this, req, res, ctxt);
      _wuiHandler->handleRequest(req, res, HttpRequestContext(&params));
    }
    return true;
  }
  // LATER optimize resource selection (avoid if/if/if)
  if (path == "/rest/csv/tasks/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvTasksView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/tasks/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTasksListView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/tasks/events/v1") { // FIXME
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTasksEventsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/hosts/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvHostsListView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/hosts/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlHostsListView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/clusters/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvClustersListView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/clusters/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlClustersListView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/resources/free/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvFreeResourcesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/resources/free/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlFreeResourcesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/resources/lwm/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvResourcesLwmView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/resources/lwm/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlResourcesLwmView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/resources/consumption/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvResourcesConsumptionView
                        ->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/resources/consumption/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlResourcesConsumptionView
                        ->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/params/global/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvGlobalParamsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/params/global/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlGlobalParamsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/alerts/params/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvAlertParamsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/alerts/params/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlAlertParamsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/alerts/raised/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvRaisedAlertsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/alerts/raised/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlRaisedAlertsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/alerts/emited/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLastEmitedAlertsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/alerts/emited/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlLastEmitedAlertsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/alerts/rules/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvAlertRulesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/alerts/rules/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlAlertRulesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/log/info/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLogView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/log/info/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlInfoLogView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/txt/log/current/v1") {
    QString path(Log::pathToLastFullestLog());
    if (path.isEmpty()) {
      res.setStatus(404);
      res.output()->write("No log file found.");
    } else {
      res.setContentType("text/plain;charset=UTF-8");
      QString filter = req.param("filter"), regexp = req.param("regexp");
      copyFilteredFile(path, res.output(), regexp.isEmpty() ? filter : regexp,
                       !regexp.isEmpty());
    }
    return true;
  }
  if (path == "/rest/txt/log/all/v1") {
    QStringList paths(Log::pathsToFullestLogs());
    if (paths.isEmpty()) {
      res.setStatus(404);
      res.output()->write("No log file found.");
    } else {
      res.setContentType("text/plain;charset=UTF-8");
      QString filter = req.param("filter"), regexp = req.param("regexp");
      copyFilteredFiles(paths, res.output(), regexp.isEmpty() ? filter : regexp,
                       !regexp.isEmpty());
    }
    return true;
  }
  if (path == "/rest/csv/taskrequests/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvTaskRequestsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/taskrequests/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTaskRequestsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/scheduler/events/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvSchedulerEventsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/scheduler/events/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlSchedulerEventsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/notices/lastposted/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLastPostedNoticesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/notices/lastposted/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlLastPostedNoticesView20->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/flags/lastchanges/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLastFlagsChangesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/flags/lastchanges/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlLastFlagsChangesView20->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/flags/set/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvFlagsSetView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/flags/set/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlFlagsSetView20->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/taskgroups/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvTaskGroupsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/taskgroups/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTaskGroupsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/taskgroups/events/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTaskGroupsEventsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/png/tasks/deploy/v1") {
    return _tasksDeploymentDiagram->handleRequest(req, res, ctxt);
  }
  if (path == "/rest/dot/tasks/deploy/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_tasksDeploymentDiagram->source(0).toUtf8());
    return true;
  }
  if (path == "/rest/png/tasks/trigger/v1") {
    return _tasksTriggerDiagram->handleRequest(req, res, ctxt);
  }
  if (path == "/rest/dot/tasks/trigger/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_tasksTriggerDiagram->source(0).toUtf8());
    return true;
  }
  res.setStatus(404);
  res.output()->write("Not found.");
  return true;
}

void WebConsole::setScheduler(Scheduler *scheduler) {
  if (_scheduler) {
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)),
               _tasksModel, SLOT(setAllTasksAndGroups(QHash<QString,TaskGroup>,QHash<QString,Task>)));
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)),
               _hostsListModel, SLOT(setAllHostsAndClusters(QHash<QString,Cluster>,QHash<QString,Host>)));
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)),
               _clustersListModel, SLOT(setAllHostsAndClusters(QHash<QString,Cluster>,QHash<QString,Host>)));
    disconnect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QHash<QString,QHash<QString,qint64> >)),
               _freeResourcesModel, SLOT(setResourceConfiguration(QHash<QString,QHash<QString,qint64> >)));
    disconnect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QHash<QString,qint64>)),
               _freeResourcesModel, SLOT(setResourceAllocationForHost(QString,QHash<QString,qint64>)));
    disconnect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QHash<QString,QHash<QString,qint64> >)),
               _resourcesLwmModel, SLOT(setResourceConfiguration(QHash<QString,QHash<QString,qint64> >)));
    disconnect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QHash<QString,qint64>)),
               _resourcesLwmModel, SLOT(setResourceAllocationForHost(QString,QHash<QString,qint64>)));
    disconnect(_scheduler, SIGNAL(taskChanged(Task)),
               _tasksModel, SLOT(taskChanged(Task)));
    disconnect(_scheduler, SIGNAL(globalParamsChanged(ParamSet)),
               _globalParamsModel, SLOT(paramsChanged(ParamSet)));
    disconnect(_scheduler->alerter(), SIGNAL(paramsChanged(ParamSet)),
               _alertParamsModel, SLOT(paramsChanged(ParamSet)));
    disconnect(_scheduler->alerter(), SIGNAL(alertRaised(QString)),
               _raisedAlertsModel, SLOT(alertRaised(QString)));
    disconnect(_scheduler->alerter(), SIGNAL(alertCanceled(QString)),
               _raisedAlertsModel, SLOT(alertCanceled(QString)));
    disconnect(_scheduler->alerter(), SIGNAL(alertCancellationScheduled(QString,QDateTime)),
               _raisedAlertsModel, SLOT(alertCancellationScheduled(QString,QDateTime)));
    disconnect(_scheduler->alerter(), SIGNAL(alertCancellationUnscheduled(QString)),
               _raisedAlertsModel, SLOT(alertCancellationUnscheduled(QString)));
    disconnect(_scheduler->alerter(), SIGNAL(alertEmited(QString)),
               this, SLOT(alertEmited(QString)));
    disconnect(_scheduler->alerter(), SIGNAL(alertCanceled(QString)),
               this, SLOT(alertCancellationEmited(QString)));
    disconnect(_scheduler->alerter(), SIGNAL(rulesChanged(QList<AlertRule>)),
               _alertRulesModel, SLOT(rulesChanged(QList<AlertRule>)));
    disconnect(_scheduler, SIGNAL(taskQueued(TaskRequest)),
               _taskRequestsHistoryModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskStarted(TaskRequest)),
               _taskRequestsHistoryModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskFinished(TaskRequest)),
               _taskRequestsHistoryModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskQueued(TaskRequest)),
               _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskStarted(TaskRequest)),
               _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskFinished(TaskRequest)),
               _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)),
               _schedulerEventsModel, SLOT(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)));
    disconnect(_scheduler, SIGNAL(noticePosted(QString)),
               _lastPostedNoticesModel, SLOT(eventOccured(QString)));
    disconnect(_scheduler, SIGNAL(flagSet(QString)),
               this, SLOT(flagSet(QString)));
    disconnect(_scheduler, SIGNAL(flagCleared(QString)),
               this, SLOT(flagCleared(QString)));
    disconnect(_scheduler, SIGNAL(flagSet(QString)),
               _flagsSetModel, SLOT(setFlag(QString)));
    disconnect(_scheduler, SIGNAL(flagCleared(QString)),
               _flagsSetModel, SLOT(clearFlag(QString)));
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)),
               _taskGroupsModel, SLOT(setAllTasksAndGroups(QHash<QString,TaskGroup>,QHash<QString,Task>)));
    disconnect(_scheduler, SIGNAL(globalParamsChanged(ParamSet)),
               this, SLOT(globalParamsChanged(ParamSet)));
    disconnect(_scheduler->alerter(), SIGNAL(channelsChanged(QStringList)),
               _alertChannelsModel, SLOT(channelsChanged(QStringList)));
    disconnect(_scheduler, SIGNAL(accessControlConfigurationChanged(bool)),
               this, SLOT(enableAccessControl(bool)));
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)),
               this, SLOT(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)));
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)),
               this, SLOT(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)));
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)),
               _resourcesConsumptionModel, SLOT(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)));
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)),
               _resourcesConsumptionModel, SLOT(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)));
    disconnect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QHash<QString,QHash<QString,qint64> >)),
               _resourcesConsumptionModel, SLOT(hostResourceConfigurationChanged(QHash<QString,QHash<QString,qint64> >)));
    disconnect(_scheduler, SIGNAL(configReloaded()),
               _resourcesConsumptionModel, SLOT(configReloaded()));
  }
  _scheduler = scheduler;
  if (_scheduler) {
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)),
            _tasksModel, SLOT(setAllTasksAndGroups(QHash<QString,TaskGroup>,QHash<QString,Task>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)),
            _hostsListModel, SLOT(setAllHostsAndClusters(QHash<QString,Cluster>,QHash<QString,Host>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)),
            _clustersListModel, SLOT(setAllHostsAndClusters(QHash<QString,Cluster>,QHash<QString,Host>)));
    connect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QHash<QString,QHash<QString,qint64> >)),
            _freeResourcesModel, SLOT(setResourceConfiguration(QHash<QString,QHash<QString,qint64> >)));
    connect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QHash<QString,qint64>)),
            _freeResourcesModel, SLOT(setResourceAllocationForHost(QString,QHash<QString,qint64>)));
    connect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QHash<QString,QHash<QString,qint64> >)),
            _resourcesLwmModel, SLOT(setResourceConfiguration(QHash<QString,QHash<QString,qint64> >)));
    connect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QHash<QString,qint64>)),
            _resourcesLwmModel, SLOT(setResourceAllocationForHost(QString,QHash<QString,qint64>)));
    connect(_scheduler, SIGNAL(taskChanged(Task)),
            _tasksModel, SLOT(taskChanged(Task)));
    connect(_scheduler, SIGNAL(globalParamsChanged(ParamSet)),
            _globalParamsModel, SLOT(paramsChanged(ParamSet)));
    connect(_scheduler->alerter(), SIGNAL(paramsChanged(ParamSet)),
            _alertParamsModel, SLOT(paramsChanged(ParamSet)));
    connect(_scheduler->alerter(), SIGNAL(alertRaised(QString)),
            _raisedAlertsModel, SLOT(alertRaised(QString)));
    connect(_scheduler->alerter(), SIGNAL(alertCanceled(QString)),
            _raisedAlertsModel, SLOT(alertCanceled(QString)));
    connect(_scheduler->alerter(), SIGNAL(alertCancellationScheduled(QString,QDateTime)),
            _raisedAlertsModel, SLOT(alertCancellationScheduled(QString,QDateTime)));
    connect(_scheduler->alerter(), SIGNAL(alertCancellationUnscheduled(QString)),
            _raisedAlertsModel, SLOT(alertCancellationUnscheduled(QString)));
    connect(_scheduler->alerter(), SIGNAL(alertEmited(QString)),
            this, SLOT(alertEmited(QString)));
    connect(_scheduler->alerter(), SIGNAL(alertCanceled(QString)),
            this, SLOT(alertCancellationEmited(QString)));
    connect(_scheduler->alerter(), SIGNAL(rulesChanged(QList<AlertRule>)),
            _alertRulesModel, SLOT(rulesChanged(QList<AlertRule>)));
    connect(_scheduler, SIGNAL(taskQueued(TaskRequest)),
            _taskRequestsHistoryModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskStarted(TaskRequest)),
            _taskRequestsHistoryModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskFinished(TaskRequest)),
            _taskRequestsHistoryModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskQueued(TaskRequest)),
            _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskStarted(TaskRequest)),
            _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskFinished(TaskRequest)),
            _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)),
            _schedulerEventsModel, SLOT(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)));
    connect(_scheduler, SIGNAL(noticePosted(QString)),
            _lastPostedNoticesModel, SLOT(eventOccured(QString)));
    connect(_scheduler, SIGNAL(flagSet(QString)),
            this, SLOT(flagSet(QString)));
    connect(_scheduler, SIGNAL(flagCleared(QString)),
            this, SLOT(flagCleared(QString)));
    connect(_scheduler, SIGNAL(flagSet(QString)),
            _flagsSetModel, SLOT(setFlag(QString)));
    connect(_scheduler, SIGNAL(flagCleared(QString)),
            _flagsSetModel, SLOT(clearFlag(QString)));
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)),
            _taskGroupsModel, SLOT(setAllTasksAndGroups(QHash<QString,TaskGroup>,QHash<QString,Task>)));
    connect(_scheduler, SIGNAL(globalParamsChanged(ParamSet)),
            this, SLOT(globalParamsChanged(ParamSet)));
    connect(_scheduler->alerter(), SIGNAL(channelsChanged(QStringList)),
            _alertChannelsModel, SLOT(channelsChanged(QStringList)));
    connect(_scheduler, SIGNAL(accessControlConfigurationChanged(bool)),
            this, SLOT(enableAccessControl(bool)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)),
            this, SLOT(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)));
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)),
            this, SLOT(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)));
    connect(_scheduler, SIGNAL(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)),
            this, SLOT(schedulerEventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)));
    connect(_scheduler, SIGNAL(configReloaded()), this, SLOT(configReloaded()));
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)),
            _resourcesConsumptionModel, SLOT(tasksConfigurationReset(QHash<QString,TaskGroup>,QHash<QString,Task>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)),
            _resourcesConsumptionModel, SLOT(targetsConfigurationReset(QHash<QString,Cluster>,QHash<QString,Host>)));
    connect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QHash<QString,QHash<QString,qint64> >)),
            _resourcesConsumptionModel, SLOT(hostResourceConfigurationChanged(QHash<QString,QHash<QString,qint64> >)));
    connect(_scheduler, SIGNAL(configReloaded()),
            _resourcesConsumptionModel, SLOT(configReloaded()));
    Log::addLogger(_memoryWarningLogger, false);
    Log::addLogger(_memoryInfoLogger, false);
  } else {
    _title = "Qron Web Console";
  }
}


void WebConsole::setUsersDatabase(UsersDatabase *usersDatabase,
                                  bool takeOwnership) {
  _authorizer->setUsersDatabase(usersDatabase, takeOwnership);
}

void WebConsole::enableAccessControl(bool enabled) {
  _accessControlEnabled = enabled;
}

void WebConsole::flagSet(QString flag) {
  emit flagChange("+"+flag, 0);
}

void WebConsole::flagCleared(QString flag) {
  emit flagChange("-"+flag, 1);
}

void WebConsole::alertEmited(QString alert) {
  emit alertEmited(alert, 0);
}

void WebConsole::alertCancellationEmited(QString alert) {
  emit alertEmited(alert+" canceled", 1);
}

void WebConsole::globalParamsChanged(ParamSet globalParams) {
  _title = globalParams.rawValue("webconsole.title", "Qron Web Console");
  _navtitle = globalParams.rawValue("webconsole.navtitle", _title);
  _cssoverload = globalParams.rawValue("webconsole.cssoverload", "");
  QString customactions_taskslist =
      globalParams.rawValue("webconsole.customactions.taskslist");
  QString customactions_requestslist =
      globalParams.rawValue("webconsole.customactions.requestslist");
  _customaction_taskdetail =
      globalParams.rawValue("webconsole.customactions.taskdetail", "");
  _tasksModel->setCustomActions(customactions_taskslist);
  _unfinishedTaskRequestModel->setCustomActions(customactions_requestslist);
  _taskRequestsHistoryModel->setCustomActions(customactions_requestslist);
}

void WebConsole::copyFilteredFiles(QStringList paths, QIODevice *output,
                                   QString pattern, bool useRegexp) {
  foreach (const QString path, paths) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
      if (pattern.isEmpty())
        IOUtils::copy(output, &file);
      else
        IOUtils::grep(output, &file, pattern, useRegexp);
    } else {
      Log::warning() << "web console cannot open log file " << path
                     << " : error #" << file.error() << " : "
                     << file.errorString();
    }
  }
}

void WebConsole::tasksConfigurationReset(QHash<QString,TaskGroup> tasksGroups,
                                         QHash<QString,Task> tasks) {
  _tasksGroups = tasksGroups;
  _tasks = tasks;
}

void WebConsole::targetsConfigurationReset(QHash<QString,Cluster> clusters,
                                           QHash<QString,Host> hosts) {
  _clusters = clusters;
  _hosts = hosts;
}

void WebConsole::schedulerEventsConfigurationReset(
    QList<Event> onstart, QList<Event> onsuccess, QList<Event> onfailure,
    QList<Event> onlog, QList<Event> onnotice, QList<Event> onschedulerstart,
    QList<Event> onconfigload) {
  _schedulerEvents.clear();
  foreach (const Event &event, onstart)
    _schedulerEvents.insertMulti("onstart", event);
  foreach (const Event &event, onsuccess)
    _schedulerEvents.insertMulti("onsuccess", event);
  foreach (const Event &event, onfailure)
    _schedulerEvents.insertMulti("onfailure", event);
  foreach (const Event &event, onlog)
    _schedulerEvents.insertMulti("onlog", event);
  foreach (const Event &event, onnotice) // LATER onnotice should be processed apart, with a notice filter
    _schedulerEvents.insertMulti("onnotice", event);
  foreach (const Event &event, onschedulerstart)
    _schedulerEvents.insertMulti("onschedulerstart", event);
  foreach (const Event &event, onconfigload)
    _schedulerEvents.insertMulti("onconfigload", event);
}

void WebConsole::configReloaded() {
  recomputeDiagrams();
}

#define CLUSTER_NODE "shape=box,peripheries=2,style=filled,fillcolor=coral"
#define HOST_NODE "shape=box,style=filled,fillcolor=coral"
#define TASK_NODE "shape=ellipse,style=filled,fillcolor=skyblue"
#define TASKGROUP_NODE "shape=ellipse,style=dashed"
#define CLUSTER_HOST_EDGE "dir=forward,arrowhead=vee"
#define TASK_TARGET_EDGE "dir=forward,arrowhead=vee"
#define TASKGROUP_TASK_EDGE "style=dashed"
#define TASKGROUP_EDGE "style=dashed"
#define NOTICE_NODE "shape=note,style=filled,fillcolor=forestgreen"
#define FLAG_NODE "shape=invtrapezium,style=\"rounded,filled\",fillcolor=gold"
#define CRON_TRIGGER_NODE "shape=none"
#define NO_TRIGGER_NODE "shape=none"
#define TASK_TRIGGER_EDGE "dir=back,arrowtail=vee"
#define TASK_NOTRIGGER_EDGE "dir=back,arrowtail=odot,style=dashed"
#define TASK_POSTNOTICE_EDGE "dir=forward,arrowhead=vee"
#define TASK_REQUESTTASK_EDGE TASK_POSTNOTICE_EDGE
#define GLOBAL_EVENT_NODE "shape=none"
//#define GLOBAL_EVENT_NODE "shape=pentagon"
#define GLOBAL_POSTNOTICE_EDGE "dir=back,arrowtail=vee"
#define GLOBAL_REQUESTTASK_EDGE TASK_POSTNOTICE_EDGE

void WebConsole::recomputeDiagrams() {
  QSet<QString> displayedGroups, displayedHosts, notices, fqtns,
      displayedGlobalEventsCause;
  // search for:
  // * displayed groups, i.e. (i) not those containing no task and (ii) also
  //   adding virtual parent groups (e.g. a group "foo.bar" as a virtual
  //   parent "foo" which is not an actual group but which make the rendering
  //   more readable
  // * displayed targets, i.e. hosts that are targets of at less one cluster
  //   or one task
  foreach (const Task &task, _tasks.values()) {
    QString s = task.taskGroup().id();
    displayedGroups.insert(s);
    for (int i = 0; (i = s.indexOf('.', i+1)) > 0; )
      displayedGroups.insert(s.mid(0, i));
    s = task.target();
    if (!s.isEmpty())
      displayedHosts.insert(s);
    fqtns.insert(task.fqtn());
  }
  foreach (const Cluster &cluster, _clusters.values())
     foreach (const Host &host, cluster.hosts())
       displayedHosts.insert(host.id());
  foreach (const Task &task, _tasks.values()) {
    notices.unite(task.noticeTriggers());
    const QMultiHash<QString,Event> events = task.allEvents();
    foreach (const Event &event, events.values()) {
      if (event.eventType() == "postnotice") {
        QString notice = event.toString();
        if (notice.size() > 1) {
          // LATER fix this ugly assumption on human readable string
          notice.remove(0, 1);
          notices.insert(notice);
        }
      }
    }
  }
  foreach (const Event &event, _schedulerEvents.values()) {
    if (event.eventType() == "postnotice") {
      QString notice = event.toString();
      if (notice.size() > 1) {
        // LATER fix this ugly assumption on human readable string
        notice.remove(0, 1);
        notices.insert(notice);
      }
    }
  }
  foreach (const QString &cause, _schedulerEvents.uniqueKeys()) {
    foreach (const Event &event, _schedulerEvents.values(cause)) {
      const QString eventType = event.eventType();
      if (eventType == "postnotice" || eventType == "requesttask")
        displayedGlobalEventsCause.insert(cause);
    }
  }
  /***************************************************************************/
  // tasks deployment diagram
  QString gv;
  gv.append("graph g {\n"
            "graph[rankdir=LR,bgcolor=\"transparent\"]\n"
            "subgraph{graph[rank=max]\n");
  foreach (const Host &host, _hosts.values())
    if (displayedHosts.contains(host.id()))
      gv.append("\"").append(host.id()).append("\"").append("[label=\"")
          .append(host.id()).append(" (")
          .append(host.hostname()).append(")\"," HOST_NODE "]\n");
  gv.append("}\n");
  foreach (const Cluster &cluster, _clusters.values()) {
    gv.append("\"").append(cluster.id()).append("\"")
        .append("[" CLUSTER_NODE "]\n");
    foreach (const Host &host, cluster.hosts())
      gv.append("\"").append(cluster.id()).append("\"--\"").append(host.id())
          .append("\"[" CLUSTER_HOST_EDGE "]\n");
  }
  gv.append("subgraph{graph[rank=min]\n");
  foreach (const QString &id, displayedGroups) {
    if (!id.contains('.')) // root groups
      gv.append("\"").append(id).append("\" [" TASKGROUP_NODE "]\n");
  }
  gv.append("}\n");
  foreach (const QString &id, displayedGroups) {
    if (id.contains('.')) // non root groups
      gv.append("\"").append(id).append("\" [" TASKGROUP_NODE "]\n");
  }
  foreach (const QString &parent, displayedGroups) {
    QString prefix = parent + ".";
    foreach (const QString &child, displayedGroups) {
      if (child.startsWith(prefix))
        gv.append("\"").append(parent).append("\" -- \"")
            .append(child).append("\" [" TASKGROUP_EDGE "]\n");
    }
  }
  foreach (const Task &task, _tasks.values()) {
    gv.append("\"").append(task.fqtn()).append("\" [label=\"")
        .append(task.id()).append("\"," TASK_NODE "]\n");
    gv.append("\"").append(task.taskGroup().id()).append("\"--")
        .append("\"").append(task.fqtn())
        .append("\" [" TASKGROUP_TASK_EDGE "]\n");
    gv.append("\"").append(task.fqtn()).append("\"--\"")
        .append(task.target()).append("\" [label=\"").append(task.mean())
        .append("\"," TASK_TARGET_EDGE "]\n");
  }
  gv.append("}");
  _tasksDeploymentDiagram->setSource(gv);
  /***************************************************************************/
  // tasks trigger diagram
  gv.clear();
  gv.append("graph g {\n"
            "graph[rankdir=LR,bgcolor=\"transparent\"]\n"
            "subgraph{graph[rank=max]\n");
  foreach (const QString &cause, displayedGlobalEventsCause)
    gv.append("\"$global_").append(cause).append("\" [label=\"")
        .append(cause).append("\"," GLOBAL_EVENT_NODE "]\n");
  gv.append("}\n");
  // LATER add flags
  foreach (QString notice, notices) {
    notice.remove('"');
    gv.append("\"$notice_").append(notice).append("\"")
        .append("[label=\"^").append(notice).append("\"," NOTICE_NODE "]\n");
  }
  gv.append("subgraph{graph[rank=min]\n");
  foreach (const QString &id, displayedGroups) {
    if (!id.contains('.')) // root groups
      gv.append("\"").append(id).append("\" [" TASKGROUP_NODE "]\n");
  }
  gv.append("}\n");
  foreach (const QString &id, displayedGroups) {
    if (id.contains('.')) // non root groups
      gv.append("\"").append(id).append("\" [" TASKGROUP_NODE "]\n");
  }
  foreach (const QString &parent, displayedGroups) {
    QString prefix = parent + ".";
    foreach (const QString &child, displayedGroups) {
      if (child.startsWith(prefix))
        gv.append("\"").append(parent).append("\" -- \"")
            .append(child).append("\" [" TASKGROUP_EDGE "]\n");
    }
  }
  int cronid = 0;
  foreach (const Task &task, _tasks.values()) {
    gv.append("\"").append(task.fqtn()).append("\" [label=\"")
        .append(task.id()).append("\"," TASK_NODE "]\n");
    gv.append("\"").append(task.taskGroup().id()).append("\"--")
        .append("\"").append(task.fqtn())
        .append("\" [" TASKGROUP_TASK_EDGE "]\n");
    foreach (const CronTrigger &cron, task.cronTriggers()) {
      gv.append("\"$cron_").append(QString::number(++cronid))
          .append("\" [label=\"(").append(cron.cronExpression())
          .append(")\"," CRON_TRIGGER_NODE "]\n");
      gv.append("\"").append(task.fqtn()).append("\"--\"$cron_")
          .append(QString::number(cronid))
          .append("\" [" TASK_TRIGGER_EDGE "]\n");
    }
    foreach (QString notice, task.noticeTriggers()) {
      notice.remove('"');
      gv.append("\"").append(task.fqtn()).append("\"--\"$notice_")
          .append(notice).append("\" [" TASK_TRIGGER_EDGE "]\n");
    }
    if (task.noticeTriggers().isEmpty() && task.cronTriggers().isEmpty()
        && task.otherTriggers().isEmpty()) {
      gv.append("\"$notrigger_").append(QString::number(++cronid))
          .append("\" [label=\"no trigger\"," NO_TRIGGER_NODE "]\n");
      gv.append("\"").append(task.fqtn()).append("\"--\"$notrigger_")
          .append(QString::number(cronid))
          .append("\" [" TASK_NOTRIGGER_EDGE "]\n");
    }
    const QMultiHash<QString,Event> events = task.allEvents();
    foreach (const QString &cause, events.uniqueKeys()) {
      foreach (const Event &event, events.values(cause)) {
        const QString eventType = event.eventType();
        if (eventType == "postnotice") {
          QString notice = event.toString();
          if (notice.size() > 1) {
            // LATER fix this ugly assumption on human readable string
            notice.remove(0, 1);
            notice.remove('"');
            gv.append("\"").append(task.fqtn()).append("\"--\"$notice_")
                .append(notice).append("\" [label=\"").append(cause)
                .append("\"," TASK_POSTNOTICE_EDGE "]\n");
          }
        } else if (eventType == "requesttask") {
          QString target = event.toString();
          if (target.size() > 1) {
            // LATER fix this ugly assumption on human readable string
            target.remove(0, 1);
            if (!target.contains('.'))
              target = task.taskGroup().id()+"."+target;
            if (fqtns.contains(target))
              gv.append("\"").append(task.fqtn()).append("\"--\"")
                  .append(target).append("\" [label=\"").append(cause)
                  .append("\"," TASK_REQUESTTASK_EDGE "]\n");
          }
        }
      }
    }
    // LATER flags (set,clear,[],[!])
  }
  foreach (const QString &cause, _schedulerEvents.uniqueKeys()) {
    foreach (const Event &event, _schedulerEvents.values(cause)) {
      const QString eventType = event.eventType();
      if (eventType == "postnotice") {
        QString notice = event.toString();
        if (notice.size() > 1) {
          // LATER fix this ugly assumption on human readable string
          notice.remove(0, 1);
          notice.remove('"');
          gv.append("\"$notice_").append(notice).append("\"--\"$global_")
              .append(cause).append("\" [" GLOBAL_POSTNOTICE_EDGE "]\n");
        }
      } else if (eventType == "requesttask") {
        QString target = event.toString();
        if (target.size() > 1) {
          // LATER fix this ugly assumption on human readable string
          target.remove(0, 1);
          if (fqtns.contains(target))
            gv.append("\"").append(target).append("\"--\"$global_")
                .append(cause).append("\" [" GLOBAL_REQUESTTASK_EDGE "]\n");
        }
      }
    }
  }
  gv.append("}");
  _tasksTriggerDiagram->setSource(gv);
  // LATER add a full events diagram with log, udp, etc. events
}
