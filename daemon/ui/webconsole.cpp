/* Copyright 2012-2014 Hallowyn and others.
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
#include "sched/taskinstance.h"
#include "sched/qrond.h"
#include <QCoreApplication>
#include "util/htmlutils.h"
#include <QUrlQuery>
#include "util/standardformats.h"
#include "textview/htmlitemdelegate.h"
#include "ui/htmltaskitemdelegate.h"
#include "ui/htmltaskinstanceitemdelegate.h"
#include "ui/htmlalertitemdelegate.h"
#include "graphvizdiagramsbuilder.h"

#define CONFIG_TABLES_MAXROWS 500
#define RAISED_ALERTS_MAXROWS 500
#define LOG_MAXROWS 500
#define SHORT_LOG_MAXROWS 100

WebConsole::WebConsole() : _thread(new QThread), _scheduler(0),
  _title("Qron Web Console"), _navtitle("Qron Web Console"), _titlehref("#"),
  _usersDatabase(0), _ownUsersDatabase(false), _accessControlEnabled(false),
  _loggersAdded(false) {
  QList<int> cols;

  // HTTP handlers
  _tasksDeploymentDiagram
      = new GraphvizImageHttpHandler(this, GraphvizImageHttpHandler::OnChange);
  _tasksDeploymentDiagram->setImageFormat(GraphvizImageHttpHandler::Svg);
  _tasksTriggerDiagram
      = new GraphvizImageHttpHandler(this, GraphvizImageHttpHandler::OnChange);
  _tasksTriggerDiagram->setImageFormat(GraphvizImageHttpHandler::Svg);
  _wuiHandler = new TemplatingHttpHandler(this, "/console", ":docroot/console");
  _wuiHandler->addFilter("\\.html$");

  // models
  _hostsListModel = new HostsListModel(this);
  _clustersListModel = new ClustersListModel(this);
  _freeResourcesModel = new HostsResourcesAvailabilityModel(this);
  _resourcesLwmModel = new HostsResourcesAvailabilityModel(
        this, HostsResourcesAvailabilityModel::LwmOverConfigured);
  _resourcesConsumptionModel = new ResourcesConsumptionModel(this);
  _globalParamsModel = new ParamSetModel(this);
  _globalSetenvModel = new ParamSetModel(this);
  _globalUnsetenvModel = new ParamSetModel(this);
  _alertParamsModel = new ParamSetModel(this);
  _raisedAlertsModel = new RaisedAlertsModel(this);
  _lastEmitedAlertsModel = new LastOccuredTextEventsModel(this, 500);
  _lastEmitedAlertsModel->setEventName("Alert")
      ->addEmptyColumn("Actions");
  connect(this, SIGNAL(alertEmited(QString,int)),
          _lastEmitedAlertsModel, SLOT(eventOccured(QString,int)));
  _lastPostedNoticesModel = new LastOccuredTextEventsModel(this, 200);
  _lastPostedNoticesModel->setEventName("Notice");
  _alertRulesModel = new AlertRulesModel(this);
  // memory cost: about 1.5 kB / instance, e.g. 30 MB for 20000 instances
  // (this is an empirical measurement and thus includes model + csv view
  _taskInstancesHistoryModel = new TaskInstancesModel(this, 20000);
  _unfinishedTaskInstancetModel = new TaskInstancesModel(this, 1000, false);
  _tasksModel = new TasksModel(this);
  _mainTasksModel = new QSortFilterProxyModel(this);
  _mainTasksModel->setFilterKeyColumn(31);
  _mainTasksModel->setFilterRegExp("^$");
  _mainTasksModel->setSourceModel(_tasksModel);
  _subtasksModel = new QSortFilterProxyModel(this);
  _subtasksModel->setFilterKeyColumn(31);
  _subtasksModel->setFilterRegExp(".+");
  _subtasksModel->setSourceModel(_tasksModel);
  _schedulerEventsModel = new SchedulerEventsModel(this);
  _taskGroupsModel = new TaskGroupsModel(this);
  _alertChannelsModel = new AlertChannelsModel(this);
  _logConfigurationModel = new LogFilesModel(this);
  _calendarsModel = new CalendarsModel(this);
  _stepsModel= new StepsModel(this);
  _memoryInfoLogger = new MemoryLogger(0, Log::Info, LOG_MAXROWS);
  _memoryWarningLogger = new MemoryLogger(0, Log::Warning, LOG_MAXROWS);

  // HTML views
  HtmlTableView::setDefaultTableClass("table table-condensed table-hover");
  _htmlHostsListView =
      new HtmlTableView(this, "hostslist", CONFIG_TABLES_MAXROWS);
  _htmlHostsListView->setModel(_hostsListModel);
  _htmlHostsListView->setEmptyPlaceholder("(no host)");
  ((HtmlItemDelegate*)_htmlHostsListView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-hdd-o\"></i> ")
      ->setPrefixForColumnHeader(2, "<i class=\"fa fa-beer\"></i> ");
  _wuiHandler->addView(_htmlHostsListView);
  _htmlClustersListView =
    new HtmlTableView(this, "clusterslist", CONFIG_TABLES_MAXROWS);
  _htmlClustersListView->setModel(_clustersListModel);
  _htmlClustersListView->setEmptyPlaceholder("(no cluster)");
  ((HtmlItemDelegate*)_htmlClustersListView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-random\"></i> ")
      ->setPrefixForColumnHeader(1, "<i class=\"fa fa-hdd-o\"></i> ");
  _wuiHandler->addView(_htmlClustersListView);
  _htmlFreeResourcesView =
    new HtmlTableView(this, "freeresources", CONFIG_TABLES_MAXROWS);
  _htmlFreeResourcesView->setModel(_freeResourcesModel);
  _htmlFreeResourcesView->setRowHeaders();
  _htmlFreeResourcesView->setEmptyPlaceholder("(no resource definition)");
  // glyphicon-celebration glyphicon-fast-food icon-glass glyphicon-fast-food
  ((HtmlItemDelegate*)_htmlFreeResourcesView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"fa fa-beer\"></i> ")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"fa fa-hdd-o\"></i> ");
  _wuiHandler->addView(_htmlFreeResourcesView);
  _htmlResourcesLwmView =
    new HtmlTableView(this, "resourceslwm", CONFIG_TABLES_MAXROWS);
  _htmlResourcesLwmView->setModel(_resourcesLwmModel);
  _htmlResourcesLwmView->setRowHeaders();
  _htmlResourcesLwmView->setEmptyPlaceholder("(no resource definition)");
  ((HtmlItemDelegate*)_htmlResourcesLwmView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"fa fa-beer\"></i> ")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"fa fa-hdd-o\"></i> ");
  _wuiHandler->addView(_htmlResourcesLwmView);
  _htmlResourcesConsumptionView =
    new HtmlTableView(this, "resourcesconsumption", CONFIG_TABLES_MAXROWS);
  _htmlResourcesConsumptionView->setModel(_resourcesConsumptionModel);
  _htmlResourcesConsumptionView->setRowHeaders();
  _htmlResourcesConsumptionView
      ->setEmptyPlaceholder("(no resource definition)");
  ((HtmlItemDelegate*)_htmlResourcesConsumptionView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"fa fa-hdd-o\"></i> ")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"fa fa-cog\"></i> ")
      ->setPrefixForRowHeader(0, "")
      ->setPrefixForRow(0, "<strong>")
      ->setSuffixForRow(0, "</strong>");
  _wuiHandler->addView(_htmlResourcesConsumptionView);
  _htmlGlobalParamsView = new HtmlTableView(this, "globalparams");
  _htmlGlobalParamsView->setModel(_globalParamsModel);
  _wuiHandler->addView(_htmlGlobalParamsView);
  _htmlGlobalSetenvView = new HtmlTableView(this, "globalsetenv");
  _htmlGlobalSetenvView->setModel(_globalSetenvModel);
  _wuiHandler->addView(_htmlGlobalSetenvView);
  _htmlGlobalUnsetenvView = new HtmlTableView(this, "globalunsetenv");
  _htmlGlobalUnsetenvView->setModel(_globalUnsetenvModel);
  _wuiHandler->addView(_htmlGlobalUnsetenvView);
  cols.clear();
  cols << 1;
  _htmlGlobalUnsetenvView->setColumnIndexes(cols);
  _htmlAlertParamsView = new HtmlTableView(this, "alertparams");
  _htmlAlertParamsView->setModel(_alertParamsModel);
  _wuiHandler->addView(_htmlAlertParamsView);
  _htmlRaisedAlertsView =
      new HtmlTableView(this, "raisedalerts", RAISED_ALERTS_MAXROWS);
  _htmlRaisedAlertsView->setModel(_raisedAlertsModel);
  _htmlRaisedAlertsView->setEmptyPlaceholder("(no alert)");
  _htmlRaisedAlertsView->setItemDelegate(
        new HtmlAlertItemDelegate(_htmlRaisedAlertsView, 0, 3, true));
  ((HtmlAlertItemDelegate*)_htmlRaisedAlertsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-bell\"></i> ");
  _wuiHandler->addView(_htmlRaisedAlertsView);
  _htmlRaisedAlertsView10 =
    new HtmlTableView(this, "raisedalerts10", RAISED_ALERTS_MAXROWS, 10);
  _htmlRaisedAlertsView10->setModel(_raisedAlertsModel);
  _htmlRaisedAlertsView10->setEmptyPlaceholder("(no alert)");
  _htmlRaisedAlertsView10->setItemDelegate(
        new HtmlAlertItemDelegate(_htmlRaisedAlertsView10, 0, 3, true));
  ((HtmlAlertItemDelegate*)_htmlRaisedAlertsView10->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-bell\"></i> ");
  _wuiHandler->addView(_htmlRaisedAlertsView10);
  _htmlLastEmitedAlertsView =
      new HtmlTableView(this, "lastemitedalerts",
                        _lastEmitedAlertsModel->maxrows());
  _htmlLastEmitedAlertsView->setModel(_lastEmitedAlertsModel);
  _htmlLastEmitedAlertsView->setEmptyPlaceholder("(no alert)");
  QHash<QString,QString> alertsIcons;
  alertsIcons.insert("0", "<i class=\"fa fa-fw fa-bell\"></i> ");
  alertsIcons.insert("1", "<i class=\"fa fa-fw fa-check\"></i> ");
  _htmlLastEmitedAlertsView->setItemDelegate(
        new HtmlAlertItemDelegate(_htmlLastEmitedAlertsView, 1, 3, false));
  ((HtmlAlertItemDelegate*)_htmlLastEmitedAlertsView->itemDelegate())
      ->setPrefixForColumn(1, "%1", 2, alertsIcons);
  cols.clear();
  cols << 0 << 1 << 3;
  _htmlLastEmitedAlertsView->setColumnIndexes(cols);
  _wuiHandler->addView(_htmlLastEmitedAlertsView);
  _htmlLastEmitedAlertsView10 =
      new HtmlTableView(this, "lastemitedalerts10",
                        _lastEmitedAlertsModel->maxrows(), 10);
  _htmlLastEmitedAlertsView10->setModel(_lastEmitedAlertsModel);
  _htmlLastEmitedAlertsView10->setEmptyPlaceholder("(no alert)");
  ((HtmlItemDelegate*)_htmlLastEmitedAlertsView10->itemDelegate())
      ->setPrefixForColumn(1, "%1", 2, alertsIcons);
  cols.clear();
  cols << 0 << 1 << 3;
  _htmlLastEmitedAlertsView10->setColumnIndexes(cols);
  _wuiHandler->addView(_htmlLastEmitedAlertsView10);
  _htmlAlertRulesView =
      new HtmlTableView(this, "alertrules", CONFIG_TABLES_MAXROWS);
  _htmlAlertRulesView->setModel(_alertRulesModel);
  QHash<QString,QString> alertRulesIcons;
  alertRulesIcons.insert("stop", "<i class=\"fa fa-stop\"></i> ");
  ((HtmlItemDelegate*)_htmlAlertRulesView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-filter\"></i> ")
      ->setPrefixForColumn(1, "%1", 1, alertRulesIcons);
  _wuiHandler->addView(_htmlAlertRulesView);
  _htmlWarningLogView = new HtmlTableView(this, "warninglog", LOG_MAXROWS, 100);
  _htmlWarningLogView->setModel(_memoryWarningLogger->model());
  _htmlWarningLogView->setEmptyPlaceholder("(empty log)");
  QHash<QString,QString> logTrClasses, logIcons;
  logTrClasses.insert("WARNING", "warning");
  logTrClasses.insert("ERROR", "error");
  logIcons.insert("WARNING", "<i class=\"fa fa-warning fa-fw\"></i> ");
  logIcons.insert("ERROR", "<i class=\"fa fa-minus-circle fa-fw\"></i> ");
  _htmlWarningLogView->setTrClass("%1", 4, logTrClasses);
  ((HtmlItemDelegate*)_htmlWarningLogView->itemDelegate())
      ->setPrefixForColumn(5, "%1", 4, logIcons);
  _wuiHandler->addView(_htmlWarningLogView);
  _htmlWarningLogView10 =
      new HtmlTableView(this, "warninglog10", SHORT_LOG_MAXROWS, 10);
  _htmlWarningLogView10->setModel(_memoryWarningLogger->model());
  _htmlWarningLogView10->setEllipsePlaceholder("(see log page for more entries)");
  _htmlWarningLogView10->setEmptyPlaceholder("(empty log)");
  _htmlWarningLogView10->setTrClass("%1", 4, logTrClasses);
  ((HtmlItemDelegate*)_htmlWarningLogView10->itemDelegate())
      ->setPrefixForColumn(5, "%1", 4, logIcons);
  _wuiHandler->addView(_htmlWarningLogView10);
  _htmlInfoLogView = new HtmlTableView(this, "infolog", LOG_MAXROWS, 100);
  _htmlInfoLogView->setModel(_memoryInfoLogger->model());
  _htmlInfoLogView->setTrClass("%1", 4, logTrClasses);
  ((HtmlItemDelegate*)_htmlInfoLogView->itemDelegate())
      ->setPrefixForColumn(5, "%1", 4, logIcons);
  _wuiHandler->addView(_htmlInfoLogView);
  _htmlWarningLogView->setEmptyPlaceholder("(empty log)");
  _htmlTaskInstancesView20 =
      new HtmlTableView(this, "taskinstances20",
                        _unfinishedTaskInstancetModel->maxrows(), 20);
  _htmlTaskInstancesView20->setModel(_unfinishedTaskInstancetModel);
  QHash<QString,QString> taskInstancesTrClasses;
  taskInstancesTrClasses.insert("failure", "error");
  taskInstancesTrClasses.insert("queued", "warning");
  taskInstancesTrClasses.insert("running", "info");
  _htmlTaskInstancesView20->setTrClass("%1", 2, taskInstancesTrClasses);
  _htmlTaskInstancesView20->setEmptyPlaceholder("(no running or queued task)");
  cols.clear();
  cols << 0 << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8;
  _htmlTaskInstancesView20->setColumnIndexes(cols);
  _htmlTaskInstancesView20
      ->setItemDelegate(new HtmlTaskInstanceItemDelegate(
                          _htmlTaskInstancesView20));
  _wuiHandler->addView(_htmlTaskInstancesView20);
  _htmlTaskInstancesView =
      new HtmlTableView(this, "taskinstances",
                        _taskInstancesHistoryModel->maxrows(), 100);
  _htmlTaskInstancesView->setModel(_taskInstancesHistoryModel);
  _htmlTaskInstancesView->setTrClass("%1", 2, taskInstancesTrClasses);
  _htmlTaskInstancesView->setEmptyPlaceholder("(no recent task)");
  cols.clear();
  cols << 0 << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8;
  _htmlTaskInstancesView->setColumnIndexes(cols);
  _htmlTaskInstancesView
      ->setItemDelegate(new HtmlTaskInstanceItemDelegate(_htmlTaskInstancesView));
  _wuiHandler->addView(_htmlTaskInstancesView);
  _htmlTasksScheduleView =
      new HtmlTableView(this, "tasksschedule", CONFIG_TABLES_MAXROWS, 100);
  _htmlTasksScheduleView->setModel(_mainTasksModel);
  _htmlTasksScheduleView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 11 << 2 << 5 << 6 << 19 << 10 << 17 << 18;
  _htmlTasksScheduleView->setColumnIndexes(cols);
  _htmlTasksScheduleView->enableRowAnchor(11);
  _htmlTasksScheduleView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksScheduleView));
  _wuiHandler->addView(_htmlTasksScheduleView);
  _htmlTasksConfigView =
      new HtmlTableView(this, "tasksconfig", CONFIG_TABLES_MAXROWS, 100);
  _htmlTasksConfigView->setModel(_tasksModel);
  _htmlTasksConfigView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 1 << 0 << 3 << 5 << 4 << 28 << 8 << 12 << 18;
  _htmlTasksConfigView->setColumnIndexes(cols);
  _htmlTasksConfigView->enableRowAnchor(11);
  _htmlTasksConfigView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksConfigView));
  _wuiHandler->addView(_htmlTasksConfigView);
  _htmlTasksParamsView =
      new HtmlTableView(this, "tasksparams", CONFIG_TABLES_MAXROWS, 100);
  _htmlTasksParamsView->setModel(_tasksModel);
  _htmlTasksParamsView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 1 << 0 << 7 << 25 << 21 << 22 << 18;
  _htmlTasksParamsView->setColumnIndexes(cols);
  _htmlTasksParamsView->enableRowAnchor(11);
  _htmlTasksParamsView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksParamsView));
  _wuiHandler->addView(_htmlTasksParamsView);
  _htmlTasksListView = // note that this view is only used in /rest urls
      new HtmlTableView(this, "taskslist", CONFIG_TABLES_MAXROWS, 100);
  _htmlTasksListView->setModel(_tasksModel);
  _htmlTasksListView->setEmptyPlaceholder("(no task in configuration)");
  _htmlTasksListView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksListView));
  _htmlTasksEventsView =
      new HtmlTableView(this, "tasksevents", CONFIG_TABLES_MAXROWS, 100);
  _htmlTasksEventsView->setModel(_mainTasksModel);
  _htmlTasksEventsView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 11 << 6 << 14 << 15 << 16 << 18;
  _htmlTasksEventsView->setColumnIndexes(cols);
  _htmlTasksEventsView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksEventsView));
  _wuiHandler->addView(_htmlTasksEventsView);
  _htmlSchedulerEventsView =
      new HtmlTableView(this, "schedulerevents", CONFIG_TABLES_MAXROWS, 100);
  ((HtmlItemDelegate*)_htmlSchedulerEventsView->itemDelegate())
      ->setPrefixForColumnHeader(0, "<i class=\"fa fa-rocket\"></i> ")
      ->setPrefixForColumnHeader(1, "<i class=\"fa fa-refresh\"></i> ")
      ->setPrefixForColumnHeader(2, "<i class=\"fa fa-comment\"></i> ")
      ->setPrefixForColumnHeader(3, "<i class=\"fa fa-file\"></i> ")
      ->setPrefixForColumnHeader(4, "<i class=\"fa fa-rocket\"></i> ")
      ->setPrefixForColumnHeader(5, "<i class=\"fa fa-flag-checkered\"></i> ")
      ->setPrefixForColumnHeader(6, "<i class=\"fa fa-minus-circle\"></i> ")
      ->setMaxCellContentLength(2000);
  _htmlSchedulerEventsView->setModel(_schedulerEventsModel);
  _wuiHandler->addView(_htmlSchedulerEventsView);
  _htmlLastPostedNoticesView20 =
      new HtmlTableView(this, "lastpostednotices20",
                        _lastPostedNoticesModel->maxrows(), 20);
  _htmlLastPostedNoticesView20->setModel(_lastPostedNoticesModel);
  _htmlLastPostedNoticesView20->setEmptyPlaceholder("(no notice)");
  cols.clear();
  cols << 0 << 1;
  _htmlLastPostedNoticesView20->setColumnIndexes(cols);
  ((HtmlItemDelegate*)_htmlLastPostedNoticesView20->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"fa fa-comment\"></i> ");
  _wuiHandler->addView(_htmlLastPostedNoticesView20);
  _htmlTaskGroupsView =
      new HtmlTableView(this, "taskgroups", CONFIG_TABLES_MAXROWS);
  _htmlTaskGroupsView->setModel(_taskGroupsModel);
  _htmlTaskGroupsView->setEmptyPlaceholder("(no task group)");
  cols.clear();
  cols << 0 << 1 << 2 << 7 << 8;
  _htmlTaskGroupsView->setColumnIndexes(cols);
  ((HtmlItemDelegate*)_htmlTaskGroupsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-cogs\"></i> ");
  _wuiHandler->addView(_htmlTaskGroupsView);
  _htmlTaskGroupsEventsView =
      new HtmlTableView(this, "taskgroupsevents", CONFIG_TABLES_MAXROWS);
  _htmlTaskGroupsEventsView->setModel(_taskGroupsModel);
  _htmlTaskGroupsEventsView->setEmptyPlaceholder("(no task group)");
  cols.clear();
  cols << 0 << 3 << 4 << 5;
  _htmlTaskGroupsEventsView->setColumnIndexes(cols);
  ((HtmlItemDelegate*)_htmlTaskGroupsEventsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-cogs\"></i> ")
      ->setPrefixForColumnHeader(3, "<i class=\"fa fa-rocket\"></i> ")
      ->setPrefixForColumnHeader(4, "<i class=\"fa fa-flag-checkered\"></i> ")
      ->setPrefixForColumnHeader(5, "<i class=\"fa fa-minus-circle\"></i> ");
  _wuiHandler->addView(_htmlTaskGroupsEventsView);
  _htmlAlertChannelsView = new HtmlTableView(this, "alertchannels");
  _htmlAlertChannelsView->setModel(_alertChannelsModel);
  _htmlAlertChannelsView->setRowHeaders();
  _wuiHandler->addView(_htmlAlertChannelsView);
  _htmlTasksResourcesView =
      new HtmlTableView(this, "tasksresources", CONFIG_TABLES_MAXROWS);
  _htmlTasksResourcesView->setModel(_tasksModel);
  _htmlTasksResourcesView->setEmptyPlaceholder("(no task)");
  cols.clear();
  cols << 11 << 17 << 8;
  _htmlTasksResourcesView->setColumnIndexes(cols);
  _htmlTasksResourcesView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksResourcesView));
  _wuiHandler->addView(_htmlTasksResourcesView);
  _htmlTasksAlertsView =
      new HtmlTableView(this, "tasksalerts", CONFIG_TABLES_MAXROWS, 100);
  _htmlTasksAlertsView->setModel(_tasksModel);
  _htmlTasksAlertsView->setEmptyPlaceholder("(no task)");
  cols.clear();
  cols << 11 << 6 << 23 << 26 << 24 << 27 << 12 << 16 << 18;
  _htmlTasksAlertsView->setColumnIndexes(cols);
  _htmlTasksAlertsView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksAlertsView));
  _wuiHandler->addView(_htmlTasksAlertsView);
  _htmlLogFilesView =
      new HtmlTableView(this, "logfiles", CONFIG_TABLES_MAXROWS);
  _htmlLogFilesView->setModel(_logConfigurationModel);
  _htmlLogFilesView->setEmptyPlaceholder("(no log file)");
  QHash<QString,QString> bufferLogFileIcons;
  bufferLogFileIcons.insert("true", "<i class=\"fa fa-download\"></i> ");
  ((HtmlItemDelegate*)_htmlLogFilesView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-file\"></i> ")
      ->setPrefixForColumn(2, "%1", 2, bufferLogFileIcons);
  _wuiHandler->addView(_htmlLogFilesView);
  _htmlCalendarsView =
      new HtmlTableView(this, "calendars", CONFIG_TABLES_MAXROWS);
  _htmlCalendarsView->setModel(_calendarsModel);
  _htmlCalendarsView->setEmptyPlaceholder("(no named calendar)");
  ((HtmlItemDelegate*)_htmlCalendarsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa fa-calendar\"></i> ");
  _wuiHandler->addView(_htmlCalendarsView);
  _htmlStepsView = new HtmlTableView(this, "steps", CONFIG_TABLES_MAXROWS);
  _htmlStepsView->setModel(_stepsModel);
  _htmlStepsView->setEmptyPlaceholder("(no workflow step)");
  cols.clear();
  cols << 0 << 2 << 3 << 4 << 6 << 7 << 8 << 9;
  _htmlStepsView->setColumnIndexes(cols);
  _htmlStepsView->enableRowAnchor(0);
  _htmlStepsView->setItemDelegate(new HtmlStepItemDelegate(_htmlStepsView));
  _wuiHandler->addView(_htmlStepsView);

  // CSV views
  CsvTableView::setDefaultFieldQuote('"');
  CsvTableView::setDefaultReplacementChar(' ');
  _csvHostsListView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvHostsListView->setModel(_hostsListModel);
  _csvClustersListView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvClustersListView->setModel(_clustersListModel);
  _csvFreeResourcesView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvFreeResourcesView->setModel(_freeResourcesModel);
  _csvFreeResourcesView->setRowHeaders();
  _csvResourcesLwmView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvResourcesLwmView->setModel(_resourcesLwmModel);
  _csvResourcesLwmView->setRowHeaders();
  _csvResourcesConsumptionView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvResourcesConsumptionView->setModel(_resourcesConsumptionModel);
  _csvResourcesConsumptionView->setRowHeaders();
  _csvGlobalParamsView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvGlobalParamsView->setModel(_globalParamsModel);
  _csvGlobalSetenvView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvGlobalSetenvView->setModel(_globalSetenvModel);
  _csvGlobalUnsetenvView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvGlobalUnsetenvView->setModel(_globalUnsetenvModel);
  _csvAlertParamsView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvAlertParamsView->setModel(_alertParamsModel);
  _csvRaisedAlertsView = new CsvTableView(this, RAISED_ALERTS_MAXROWS);
  _csvRaisedAlertsView->setModel(_raisedAlertsModel);
  _csvLastEmitedAlertsView =
      new CsvTableView(this, _lastEmitedAlertsModel->maxrows());
  _csvLastEmitedAlertsView->setModel(_lastEmitedAlertsModel);
  _csvAlertRulesView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvAlertRulesView->setModel(_alertRulesModel);
  _csvLogView = new CsvTableView(this, _htmlInfoLogView->cachedRows());
  _csvLogView->setModel(_memoryInfoLogger->model());
  _csvTaskInstancesView =
        new CsvTableView(this, _taskInstancesHistoryModel->maxrows());
  _csvTaskInstancesView->setModel(_taskInstancesHistoryModel);
  _csvTasksView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvTasksView->setModel(_tasksModel);
  _csvSchedulerEventsView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvSchedulerEventsView->setModel(_schedulerEventsModel);
  _csvLastPostedNoticesView =
      new CsvTableView(this, _lastPostedNoticesModel->maxrows());
  _csvLastPostedNoticesView->setModel(_lastPostedNoticesModel);
  _csvTaskGroupsView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvTaskGroupsView->setModel(_taskGroupsModel);
  _csvLogFilesView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvLogFilesView->setModel(_logConfigurationModel);
  _csvCalendarsView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvCalendarsView->setModel(_calendarsModel);
  _csvStepsView = new CsvTableView(this, CONFIG_TABLES_MAXROWS);
  _csvStepsView->setModel(_stepsModel);

  // other views
  _clockView = new ClockView(this);
  _clockView->setFormat("yyyy-MM-dd hh:mm:ss,zzz");
  _wuiHandler->addView("now", _clockView);

  // access control
  _authorizer = new InMemoryRulesAuthorizer(this);
  _authorizer->allow("", "/console/(css|jsp|js)/.*") // anyone for static rsrc
      .allow("operate", "/(rest|console)/do") // operate for operation
      .deny("", "/(rest|console)/do") // nobody else
      .allow("read"); // read for everything else

  // dedicated thread
  _thread->setObjectName("WebConsoleServer");
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
  _memoryInfoLogger->moveToThread(_thread); // TODO won't be deleted whereas they should
  _memoryWarningLogger->moveToThread(_thread);
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
        alertType = message.at(0).toLatin1();
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
    if (key == "titlehref") // TODO remove
      return _console->_titlehref;
    if (key == "cssoverload") // TODO remove
      return _console->_cssoverload;
    if (key == "message")
      return _message;
    if (key == "startdate")
      return _console->_scheduler->startdate().toString("yyyy-MM-dd hh:mm:ss");
    if (key == "uptime")
      return StandardFormats::toCoarseHumanReadableTimeInterval(
            _console->_scheduler->startdate()
            .msecsTo(QDateTime::currentDateTime()));
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
    url.setQuery(QString());
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
        // FXIME evaluate overriding params within overriding > global context
        QList<TaskInstance> instances = _scheduler->syncRequestTask(fqtn, params);
        if (!instances.isEmpty()) {
          message = "S:Task '"+fqtn+"' submitted for execution with id";
          foreach (TaskInstance request, instances)
              message.append(' ').append(QString::number(request.id()));
          message.append('.');
        } else
          message = "E:Execution request of task '"+fqtn
              +"' failed (see logs for more information).";
      } else if (event == "cancelRequest") {
        TaskInstance instance = _scheduler->cancelRequest(id);
        if (!instance.isNull())
          message = "S:Task request "+QString::number(id)+" canceled.";
        else
          message = "E:Cannot cancel request "+QString::number(id)+".";
      } else if (event == "abortTask") {
        TaskInstance instance = _scheduler->abortTask(id);
        if (!instance.isNull())
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
      url.setQuery(QString());
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
                        +task.triggersWithCalendarsAsString()+"</td></tr>"
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
                                 : "<i class=\"fa fa-ban\"></i> false")
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
                        +(instancesCount ? "<i class=\"fa fa-play\"></i> " : "")
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
                        +HtmlUtils::htmlEncode(task.params().toString(false,
                                                                      false))
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
                        +EventSubscription::toStringList(task.onstartEventSubscriptions())
                        .join("\n")+"</td></tr>"
                        "<tr><th>Onsuccess events</th><td>"
                        +EventSubscription::toStringList(task.onsuccessEventSubscriptions())
                        .join("\n")+"</td></tr>"
                        "<tr><th>Onfailure events</th><td>"
                        +EventSubscription::toStringList(task.onfailureEventSubscriptions())
                        .join("\n")+"</td></tr>");
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
    QList<QPair<QString,QString> > queryItems(req.urlQuery().queryItems());
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
  if (path == "/rest/csv/steps/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvStepsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/steps/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlStepsView->text().toUtf8().constData());
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
  if (path == "/rest/csv/setenv/global/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvGlobalSetenvView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/setenv/global/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlGlobalSetenvView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/unsetenv/global/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvGlobalUnsetenvView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/unsetenv/global/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlGlobalUnsetenvView->text().toUtf8().constData());
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
  if (path == "/rest/csv/taskinstances/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvTaskInstancesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/taskinstances/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTaskInstancesView->text().toUtf8().constData());
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
  if (path == "/rest/svg/tasks/deploy/v1") {
    return _tasksDeploymentDiagram->handleRequest(req, res, ctxt);
  }
  if (path == "/rest/dot/tasks/deploy/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_tasksDeploymentDiagram->source(0).toUtf8());
    return true;
  }
  if (path == "/rest/svg/tasks/workflow/v1") {
    // LATER this rendering should be pooled
    if (_scheduler) {
      Task task = _scheduler->task(req.param("fqtn"));
      QString gv = task.workflowDiagram();
      if (!gv.isEmpty()) {
        GraphvizImageHttpHandler *h = new GraphvizImageHttpHandler;
        h->setImageFormat(GraphvizImageHttpHandler::Svg);
        h->setSource(gv);
        h->handleRequest(req, res, ctxt);
        h->deleteLater();
        return true;
      }
    }
    res.setStatus(404);
    res.output()->write("No such workflow.");
    return true;
  }
  if (path == "/rest/dot/tasks/workflow/v1") {
    if (_scheduler) {
      Task task = _scheduler->task(req.param("fqtn"));
      QString gv = task.workflowDiagram();
      if (!gv.isEmpty()) {
        res.setContentType("text/html;charset=UTF-8");
        res.output()->write(gv.toUtf8());
        return true;
      }
    }
    res.setStatus(404);
    res.output()->write("No such workflow.");
    return true;
  }
  if (path == "/rest/svg/tasks/trigger/v1") {
    return _tasksTriggerDiagram->handleRequest(req, res, ctxt);
  }
  if (path == "/rest/dot/tasks/trigger/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_tasksTriggerDiagram->source(0).toUtf8());
    return true;
  }
  if (path == "/rest/csv/log/files/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLogFilesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/log/files/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlLogFilesView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/csv/calendars/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvCalendarsView->text().toUtf8().constData());
    return true;
  }
  if (path == "/rest/html/calendars/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlCalendarsView->text().toUtf8().constData());
    return true;
  }
  res.setStatus(404);
  res.output()->write("Not found.");
  return true;
}

void WebConsole::setScheduler(Scheduler *scheduler) {
  if (_scheduler) {
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _tasksModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _hostsListModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _clustersListModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _freeResourcesModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(hostsResourcesAvailabilityChanged(QString,QHash<QString,qint64>)),
               _freeResourcesModel, SLOT(hostsResourcesAvailabilityChanged(QString,QHash<QString,qint64>)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _resourcesLwmModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(hostsResourcesAvailabilityChanged(QString,QHash<QString,qint64>)),
               _resourcesLwmModel, SLOT(hostsResourcesAvailabilityChanged(QString,QHash<QString,qint64>)));
    disconnect(_scheduler, SIGNAL(taskChanged(Task)),
               _tasksModel, SLOT(taskChanged(Task)));
    disconnect(_scheduler, SIGNAL(globalParamsChanged(ParamSet)),
               _globalParamsModel, SLOT(paramsChanged(ParamSet)));
    disconnect(_scheduler, SIGNAL(globalSetenvChanged(ParamSet)),
               _globalSetenvModel, SLOT(paramsChanged(ParamSet)));
    disconnect(_scheduler, SIGNAL(globalUnsetenvChanged(ParamSet)),
               _globalUnsetenvModel, SLOT(paramsChanged(ParamSet)));
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
    disconnect(_scheduler->alerter(), SIGNAL(configChanged(AlerterConfig)),
               _alertRulesModel, SLOT(configChanged(AlerterConfig)));
    disconnect(_scheduler, SIGNAL(taskQueued(TaskInstance)),
               _taskInstancesHistoryModel, SLOT(taskChanged(TaskInstance)));
    disconnect(_scheduler, SIGNAL(taskStarted(TaskInstance)),
               _taskInstancesHistoryModel, SLOT(taskChanged(TaskInstance)));
    disconnect(_scheduler, SIGNAL(taskFinished(TaskInstance)),
               _taskInstancesHistoryModel, SLOT(taskChanged(TaskInstance)));
    disconnect(_scheduler, SIGNAL(taskQueued(TaskInstance)),
               _unfinishedTaskInstancetModel, SLOT(taskChanged(TaskInstance)));
    disconnect(_scheduler, SIGNAL(taskStarted(TaskInstance)),
               _unfinishedTaskInstancetModel, SLOT(taskChanged(TaskInstance)));
    disconnect(_scheduler, SIGNAL(taskFinished(TaskInstance)),
               _unfinishedTaskInstancetModel, SLOT(taskChanged(TaskInstance)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _schedulerEventsModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _calendarsModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(noticePosted(QString,ParamSet)),
               _lastPostedNoticesModel, SLOT(eventOccured(QString)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _taskGroupsModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               this, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler->alerter(), SIGNAL(configChanged(AlerterConfig)),
               _alertChannelsModel, SLOT(configChanged(AlerterConfig)));
    disconnect(_scheduler, SIGNAL(accessControlConfigurationChanged(bool)),
               this, SLOT(enableAccessControl(bool)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _resourcesConsumptionModel, SLOT(configChanged(SchedulerConfig)));
    disconnect(_scheduler, SIGNAL(logConfigurationChanged(QList<LogFile>)),
               _logConfigurationModel, SLOT(logConfigurationChanged(QList<LogFile>)));
    disconnect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
               _stepsModel, SLOT(configChanged(SchedulerConfig)));
  }
  _scheduler = scheduler;
  if (_scheduler) {
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _tasksModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _hostsListModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _clustersListModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _freeResourcesModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(hostsResourcesAvailabilityChanged(QString,QHash<QString,qint64>)),
            _freeResourcesModel, SLOT(hostsResourcesAvailabilityChanged(QString,QHash<QString,qint64>)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _resourcesLwmModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(hostsResourcesAvailabilityChanged(QString,QHash<QString,qint64>)),
            _resourcesLwmModel, SLOT(hostsResourcesAvailabilityChanged(QString,QHash<QString,qint64>)));
    connect(_scheduler, SIGNAL(taskChanged(Task)),
            _tasksModel, SLOT(taskChanged(Task)));
    connect(_scheduler, SIGNAL(globalParamsChanged(ParamSet)),
            _globalParamsModel, SLOT(paramsChanged(ParamSet)));
    connect(_scheduler, SIGNAL(globalSetenvChanged(ParamSet)),
            _globalSetenvModel, SLOT(paramsChanged(ParamSet)));
    connect(_scheduler, SIGNAL(globalUnsetenvChanged(ParamSet)),
            _globalUnsetenvModel, SLOT(paramsChanged(ParamSet)));
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
    connect(_scheduler->alerter(), SIGNAL(configChanged(AlerterConfig)),
            _alertRulesModel, SLOT(configChanged(AlerterConfig)));
    connect(_scheduler, SIGNAL(taskQueued(TaskInstance)),
            _taskInstancesHistoryModel, SLOT(taskChanged(TaskInstance)));
    connect(_scheduler, SIGNAL(taskStarted(TaskInstance)),
            _taskInstancesHistoryModel, SLOT(taskChanged(TaskInstance)));
    connect(_scheduler, SIGNAL(taskFinished(TaskInstance)),
            _taskInstancesHistoryModel, SLOT(taskChanged(TaskInstance)));
    connect(_scheduler, SIGNAL(taskQueued(TaskInstance)),
            _unfinishedTaskInstancetModel, SLOT(taskChanged(TaskInstance)));
    connect(_scheduler, SIGNAL(taskStarted(TaskInstance)),
            _unfinishedTaskInstancetModel, SLOT(taskChanged(TaskInstance)));
    connect(_scheduler, SIGNAL(taskFinished(TaskInstance)),
            _unfinishedTaskInstancetModel, SLOT(taskChanged(TaskInstance)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _schedulerEventsModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _calendarsModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(noticePosted(QString,ParamSet)),
            _lastPostedNoticesModel, SLOT(eventOccured(QString)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _taskGroupsModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            this, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler->alerter(), SIGNAL(configChanged(AlerterConfig)),
            _alertChannelsModel, SLOT(configChanged(AlerterConfig)));
    connect(_scheduler, SIGNAL(accessControlConfigurationChanged(bool)),
            this, SLOT(enableAccessControl(bool)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _resourcesConsumptionModel, SLOT(configChanged(SchedulerConfig)));
    connect(_scheduler, SIGNAL(logConfigurationChanged(QList<LogFile>)),
            _logConfigurationModel, SLOT(logConfigurationChanged(QList<LogFile>)));
    connect(_scheduler, SIGNAL(configChanged(SchedulerConfig)),
            _stepsModel, SLOT(configChanged(SchedulerConfig)));
  } else {
    _title = "Qron Web Console";
  }
  if (!_loggersAdded) {
    _loggersAdded = true;
    Log::addLogger(_memoryWarningLogger, false);
    Log::addLogger(_memoryInfoLogger, false);
  }
}


void WebConsole::setUsersDatabase(UsersDatabase *usersDatabase,
                                  bool takeOwnership) {
  _authorizer->setUsersDatabase(usersDatabase, takeOwnership);
}

void WebConsole::enableAccessControl(bool enabled) {
  _accessControlEnabled = enabled;
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
  _titlehref = globalParams.rawValue("webconsole.titlehref", _titlehref);
  _cssoverload = globalParams.rawValue("webconsole.cssoverload", "");
  QString customactions_taskslist =
      globalParams.rawValue("webconsole.customactions.taskslist");
  QString customactions_instanceslist =
      globalParams.rawValue("webconsole.customactions.instanceslist");
  _customaction_taskdetail =
      globalParams.rawValue("webconsole.customactions.taskdetail", "");
  _tasksModel->setCustomActions(customactions_taskslist);
  _unfinishedTaskInstancetModel->setCustomActions(customactions_instanceslist);
  _taskInstancesHistoryModel->setCustomActions(customactions_instanceslist);
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

void WebConsole::configChanged(SchedulerConfig config) {
  globalParamsChanged(config.globalParams());
  QHash<QString,QString> diagrams
      = GraphvizDiagramsBuilder::configDiagrams(config);
  _tasksDeploymentDiagram->setSource(diagrams.value("tasksDeploymentDiagram"));
  _tasksTriggerDiagram->setSource(diagrams.value("tasksTriggerDiagram"));
}
