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
#include "webconsole.h"
#include "util/ioutils.h"
#include <QFile>
#include <QThread>
#include "sched/taskinstance.h"
#include "sched/qrond.h"
#include <QCoreApplication>
#include "format/stringutils.h"
#include <QUrlQuery>
#include "format/timeformats.h"
#include "textview/htmlitemdelegate.h"
#include "htmltaskitemdelegate.h"
#include "htmltaskinstanceitemdelegate.h"
#include "htmlalertitemdelegate.h"
#include "ui/graphvizdiagramsbuilder.h"
#include "htmllogentryitemdelegate.h"
#include "alert/alert.h"
#include "alert/gridboard.h"
#include "util/radixtree.h"
#include <functional>
#include "util/characterseparatedexpression.h"
#include "format/htmltableformatter.h"
#include "format/jsonformats.h"
#include <QJsonDocument>
#include "config/requestformfield.h"

#define SHORT_LOG_MAXROWS 100
#define SHORT_LOG_ROWSPERPAGE 10
#define TASK_INSTANCE_HISTORY_MAXROWS 10000
#define UNFINISHED_TASK_INSTANCE_MAXROWS 10000
#define ISO8601 QStringLiteral("yyyy-MM-dd hh:mm:ss")
//#define GRAPHVIZ_MIME_TYPE "text/vnd.graphviz;charset=UTF-8"
#define GRAPHVIZ_MIME_TYPE "text/plain;charset=UTF-8"

static PfNode nodeWithValidPattern =
    PfNode("dummy", "dummy").setAttribute("pattern", ".*")
    .setAttribute("dimension", "dummy");
static QRegularExpression htmlSuffixRe("\\.html$");
static QRegularExpression pfSuffixRe("\\.pf$");
static CsvFormatter _csvFormatter(',', "\n", '"', '\0', ' ', -1);
static HtmlTableFormatter _htmlTableFormatter(-1);

// syntaxic sugar to define "a||b" as "a" if not empty and "b" otherwise
static inline QString operator||(QString a, QString b) {
  return a.isEmpty() ? b : a;
}

WebConsole::WebConsole() : _thread(new QThread), _scheduler(0),
  _configRepository(0), _authorizer(0),
  _readOnlyResourcesCache(new ReadOnlyResourcesCache(this)) {

  // HTTP handlers
  _tasksDeploymentDiagram
      = new GraphvizImageHttpHandler(this, GraphvizImageHttpHandler::OnChange);
  _tasksDeploymentDiagram->setImageFormat(GraphvizImageHttpHandler::Svg);
  _tasksTriggerDiagram
      = new GraphvizImageHttpHandler(this, GraphvizImageHttpHandler::OnChange);
  _tasksTriggerDiagram->setImageFormat(GraphvizImageHttpHandler::Svg);
  _tasksResourcesHostsDiagram
      = new GraphvizImageHttpHandler(this, GraphvizImageHttpHandler::OnChange);
  _tasksResourcesHostsDiagram->setImageFormat(GraphvizImageHttpHandler::Svg);
  _wuiHandler = new TemplatingHttpHandler(this, "/console", ":docroot/console");
  _wuiHandler->addFilter("\\.html$");
  _configUploadHandler = new ConfigUploadHandler("", 1, this);

  // models
  _hostsModel = new SharedUiItemsTableModel(Host(PfNode("host"), ParamSet()),
                                            this);
  _hostsModel->setItemQualifierFilter("host");
  _sortedHostsModel = new QSortFilterProxyModel(this);
  _sortedHostsModel->sort(0);
  _sortedHostsModel->setSourceModel(_hostsModel);
  _clustersModel = new ClustersModel(this);
  _clustersModel->setItemQualifierFilter({"cluster", "hostreference"});
  _sortedClustersModel = new QSortFilterProxyModel(this);
  _sortedClustersModel->sort(0);
  _sortedClustersModel->setSourceModel(_clustersModel);
  _freeResourcesModel = new HostsResourcesAvailabilityModel(this);
  _freeResourcesModel->enableRowsSort();
  _freeResourcesModel->enableColumnsSort();
  _resourcesLwmModel = new HostsResourcesAvailabilityModel(
        this, HostsResourcesAvailabilityModel::LwmOverConfigured);
  _resourcesLwmModel->enableRowsSort();
  _resourcesLwmModel->enableColumnsSort();
  _resourcesConsumptionModel = new ResourcesConsumptionModel(this);
  _globalParamsModel = new ParamSetModel(this);
  _globalVarsModel = new ParamSetModel(this);
  _alertParamsModel = new ParamSetModel(this);
  _statefulAlertsModel = new SharedUiItemsTableModel(this);
  _statefulAlertsModel->setHeaderDataFromTemplate(Alert("template"));
  _statefulAlertsModel->setDefaultInsertionPoint(
        SharedUiItemsTableModel::FirstItem);
  _sortedStatefulAlertsModel = new QSortFilterProxyModel(this);
  _sortedStatefulAlertsModel->sort(0);
  _sortedStatefulAlertsModel->setSourceModel(_statefulAlertsModel);
  _sortedRaisedAlertModel = new QSortFilterProxyModel(this);
  _sortedRaisedAlertModel->sort(0);
  _sortedRaisedAlertModel->setFilterKeyColumn(1);
  _sortedRaisedAlertModel->setFilterRegularExpression("raised|dropping");
  _sortedRaisedAlertModel->setSourceModel(_statefulAlertsModel);
  _lastEmittedAlertsModel = new SharedUiItemsLogModel(this, 500);
  _lastEmittedAlertsModel->setHeaderDataFromTemplate(
              Alert("template"), Qt::DisplayRole);
  _lastPostedNoticesModel = new LastOccuredTextEventsModel(this, 200);
  _lastPostedNoticesModel->setEventName("Notice");
  _alertSubscriptionsModel = new SharedUiItemsTableModel(this);
  _alertSubscriptionsModel->setHeaderDataFromTemplate(
        AlertSubscription(nodeWithValidPattern, PfNode(), ParamSet()));
  _alertSettingsModel = new SharedUiItemsTableModel(this);
  _alertSettingsModel->setHeaderDataFromTemplate(
        AlertSettings(nodeWithValidPattern));
  _gridboardsModel = new SharedUiItemsTableModel(this);
  _gridboardsModel->setHeaderDataFromTemplate(
        Gridboard(nodeWithValidPattern, Gridboard(), ParamSet()));
  _sortedGridboardsModel = new QSortFilterProxyModel(this);
  _sortedGridboardsModel->sort(0);
  _sortedGridboardsModel->setSourceModel(_gridboardsModel);
  _taskInstancesHistoryModel =
      new TaskInstancesModel(this, TASK_INSTANCE_HISTORY_MAXROWS);
  _taskInstancesHistoryModel->setItemQualifierFilter("taskinstance");
  _unfinishedTaskInstancesModel =
      new TaskInstancesModel(this, UNFINISHED_TASK_INSTANCE_MAXROWS, false);
  _unfinishedTaskInstancesModel->setItemQualifierFilter("taskinstance");
  _herdsHistoryModel = new QSortFilterProxyModel(this);
  _herdsHistoryModel->setFilterKeyColumn(11);
  _herdsHistoryModel->setFilterRegularExpression(".");
  _herdsHistoryModel->setSourceModel(_taskInstancesHistoryModel);
  _tasksModel = new TasksModel(this);
  _tasksModel->setItemQualifierFilter("task");
  _schedulerEventsModel = new SchedulerEventsModel(this);
  _taskGroupsModel = new TaskGroupsModel(this);
  _taskGroupsModel->setItemQualifierFilter("taskgroup");
  _sortedTaskGroupsModel = new QSortFilterProxyModel(this);
  _sortedTaskGroupsModel->sort(0);
  _sortedTaskGroupsModel->setSourceModel(_taskGroupsModel);
  _alertChannelsModel = new TextMatrixModel(this);
  _logConfigurationModel = new SharedUiItemsTableModel(
        LogFile({ "log", { { "file", "/tmp/foobar" } } }), this);
  _calendarsModel = new SharedUiItemsTableModel(this);
  _calendarsModel->setHeaderDataFromTemplate(Calendar(PfNode("calendar")));
  _calendarsModel->setItemQualifierFilter("calendar");
  _sortedCalendarsModel = new QSortFilterProxyModel(this);
  _sortedCalendarsModel->sort(1);
  _sortedCalendarsModel->setSourceModel(_calendarsModel);
  _warningLogModel = new LogModel(this, Log::Warning);
  _infoLogModel = new LogModel(this, Log::Info);
  _auditLogModel = new LogModel(this, Log::Info, "AUDIT ");
  _configsModel = new ConfigsModel(this);
  _configHistoryModel = new ConfigHistoryModel(this);

  // HTML views
  HtmlTableView::setDefaultTableClass("table table-condensed table-hover");
  _htmlHostsListView = new HtmlTableView(this, "hostslist");
  _htmlHostsListView->setModel(_sortedHostsModel);
  _htmlHostsListView->setEmptyPlaceholder("(no host)");
  qobject_cast<HtmlItemDelegate*>(_htmlHostsListView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa-solid fa-hard-drive\"></i>&nbsp;")
      ->setPrefixForColumnHeader(2, "<i class=\"fa-solid fa-wheat-awn\"></i>&nbsp;");
  _wuiHandler->addView(_htmlHostsListView);
  _htmlClustersListView = new HtmlTableView(this, "clusterslist");
  _htmlClustersListView->setModel(_sortedClustersModel);
  _htmlClustersListView->setEmptyPlaceholder("(no cluster)");
  qobject_cast<HtmlItemDelegate*>(_htmlClustersListView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa-solid fa-shuffle\"></i>&nbsp;")
      ->setPrefixForColumnHeader(1, "<i class=\"fa-solid fa-hard-drive\"></i>&nbsp;");
  _wuiHandler->addView(_htmlClustersListView);
  _htmlFreeResourcesView = new HtmlTableView(this, "freeresources");
  _htmlFreeResourcesView->setModel(_freeResourcesModel);
  _htmlFreeResourcesView->enableRowHeaders();
  _htmlFreeResourcesView->setEmptyPlaceholder("(no resource definition)");
  qobject_cast<HtmlItemDelegate*>(_htmlFreeResourcesView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"fa-solid fa-wheat-awn\"></i>&nbsp;")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"fa-solid fa-hard-drive\"></i>&nbsp;");
  _wuiHandler->addView(_htmlFreeResourcesView);
  _htmlResourcesLwmView = new HtmlTableView(this, "resourceslwm");
  _htmlResourcesLwmView->setModel(_resourcesLwmModel);
  _htmlResourcesLwmView->enableRowHeaders();
  _htmlResourcesLwmView->setEmptyPlaceholder("(no resource definition)");
  qobject_cast<HtmlItemDelegate*>(_htmlResourcesLwmView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"fa-solid fa-wheat-awn\"></i>&nbsp;")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"fa-solid fa-hard-drive\"></i>&nbsp;");
  _wuiHandler->addView(_htmlResourcesLwmView);
  _htmlResourcesConsumptionView =
      new HtmlTableView(this, "resourcesconsumption");
  _htmlResourcesConsumptionView->setModel(_resourcesConsumptionModel);
  _htmlResourcesConsumptionView->enableRowHeaders();
  _htmlResourcesConsumptionView
      ->setEmptyPlaceholder("(no resource definition)");
  qobject_cast<HtmlItemDelegate*>(_htmlResourcesConsumptionView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"fa-solid fa-hard-drive\"></i>&nbsp;")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"fa-solid fa-gear\"></i>&nbsp;")
      ->setPrefixForRowHeader(0, "")
      ->setPrefixForRow(0, "<strong>")
      ->setSuffixForRow(0, "</strong>");
  _wuiHandler->addView(_htmlResourcesConsumptionView);
  _htmlGlobalParamsView = new HtmlTableView(this, "globalparams");
  _htmlGlobalParamsView->setModel(_globalParamsModel);
  _htmlGlobalParamsView->setColumnIndexes({0,1});
  _wuiHandler->addView(_htmlGlobalParamsView);
  _htmlGlobalVarsView = new HtmlTableView(this, "globalvars");
  _htmlGlobalVarsView->setModel(_globalVarsModel);
  _htmlGlobalVarsView->setColumnIndexes({0,1});
  _wuiHandler->addView(_htmlGlobalVarsView);
  _htmlAlertParamsView = new HtmlTableView(this, "alertparams");
  _htmlAlertParamsView->setModel(_alertParamsModel);
  _wuiHandler->addView(_htmlAlertParamsView);
  _htmlStatefulAlertsView = new HtmlTableView(this, "statefulalerts");
  _htmlStatefulAlertsView->setModel(_sortedStatefulAlertsModel);
  _htmlStatefulAlertsView->setEmptyPlaceholder("(no alert)");
  _htmlStatefulAlertsView->setColumnIndexes({0,2,3,4,5});
  QHash<QString,QString> alertsIcons;
  alertsIcons.insert(Alert::statusAsString(Alert::Nonexistent),
                     "<i class=\"fa-solid fa-bell\"></i>&nbsp;");
  alertsIcons.insert(Alert::statusAsString(Alert::Rising),
                     "<i class=\"fa-regular fa-bell\"></i>&nbsp;<strike>");
  alertsIcons.insert(Alert::statusAsString(Alert::MayRise),
                     "<i class=\"fa-regular fa-bell\"></i>&nbsp;<strike>");
  alertsIcons.insert(Alert::statusAsString(Alert::Raised),
                     "<i class=\"fa-solid fa-bell\"></i>&nbsp;");
  alertsIcons.insert(Alert::statusAsString(Alert::Dropping),
                     "<i class=\"fa-solid fa-bell\"></i>&nbsp;");
  alertsIcons.insert(Alert::statusAsString(Alert::Canceled),
                     "<i class=\"fa-solid fa-check\"></i>&nbsp;");
  _htmlStatefulAlertsView->setItemDelegate(
        new HtmlAlertItemDelegate(_htmlStatefulAlertsView, true));
  qobject_cast<HtmlItemDelegate*>(_htmlStatefulAlertsView->itemDelegate())
      ->setPrefixForColumn(0, "%1", 1, alertsIcons);
  _wuiHandler->addView(_htmlStatefulAlertsView);
  _htmlRaisedAlertsView = new HtmlTableView(this, "raisedalerts");
  _htmlRaisedAlertsView->setRowsPerPage(10);
  _htmlRaisedAlertsView->setModel(_sortedRaisedAlertModel);
  _htmlRaisedAlertsView->setEmptyPlaceholder("(no alert)");
  _htmlRaisedAlertsView->setColumnIndexes({0,2,4,5});
  _htmlRaisedAlertsView->setItemDelegate(
        new HtmlAlertItemDelegate(_htmlRaisedAlertsView, true));
  qobject_cast<HtmlItemDelegate*>(_htmlRaisedAlertsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa-solid fa-bell\"></i>&nbsp;");
  _wuiHandler->addView(_htmlRaisedAlertsView);
  _htmlLastEmittedAlertsView =
      new HtmlTableView(this, "lastemittedalerts",
                        _lastEmittedAlertsModel->maxrows());
  _htmlLastEmittedAlertsView->setModel(_lastEmittedAlertsModel);
  _htmlLastEmittedAlertsView->setEmptyPlaceholder("(no alert)");
  _htmlLastEmittedAlertsView->setItemDelegate(
        new HtmlAlertItemDelegate(_htmlLastEmittedAlertsView, false));
  qobject_cast<HtmlItemDelegate*>(_htmlLastEmittedAlertsView->itemDelegate())
      ->setPrefixForColumn(7, "%1", 1, alertsIcons);
  _htmlLastEmittedAlertsView->setColumnIndexes(
        {_lastEmittedAlertsModel->timestampColumn(),7,5});
  _wuiHandler->addView(_htmlLastEmittedAlertsView);
  _htmlAlertSubscriptionsView = new HtmlTableView(this, "alertsubscriptions");
  _htmlAlertSubscriptionsView->setModel(_alertSubscriptionsModel);
  QHash<QString,QString> alertSubscriptionsIcons;
  alertSubscriptionsIcons.insert("stop", "<i class=\"fa-solid fa-stop\"></i>&nbsp;");
  qobject_cast<HtmlItemDelegate*>(_htmlAlertSubscriptionsView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"fa-solid fa-filter\"></i>&nbsp;")
      ->setPrefixForColumn(2, "%1", 2, alertSubscriptionsIcons);
  _htmlAlertSubscriptionsView->setColumnIndexes({1,2,3,4,5,12});
  _wuiHandler->addView(_htmlAlertSubscriptionsView);
  _htmlAlertSettingsView = new HtmlTableView(this, "alertsettings");
  _htmlAlertSettingsView->setModel(_alertSettingsModel);
  qobject_cast<HtmlItemDelegate*>(_htmlAlertSettingsView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"fa-solid fa-filter\"></i>&nbsp;");
  _htmlAlertSettingsView->setColumnIndexes({1,2});
  _wuiHandler->addView(_htmlAlertSettingsView);
  _htmlGridboardsView = new HtmlTableView(this, "gridboards");
  _htmlGridboardsView->setModel(_sortedGridboardsModel);
  qobject_cast<HtmlItemDelegate*>(_htmlGridboardsView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"fa-solid fa-table-cells\"></i>&nbsp;"
                              "<a href=\"gridboards/%1\">", 0)
      ->setSuffixForColumn(1, "</a>");
  _htmlGridboardsView->setColumnIndexes({1,2,3});
  _wuiHandler->addView(_htmlGridboardsView);
  _htmlWarningLogView = new HtmlTableView(this, "warninglog");
  _htmlWarningLogView->setModel(_warningLogModel);
  _htmlWarningLogView->setEmptyPlaceholder("(empty log)");
  QHash<QString,QString> logTrClasses;
  logTrClasses.insert(Log::severityToString(Log::Warning), "warning");
  logTrClasses.insert(Log::severityToString(Log::Error), "danger");
  logTrClasses.insert(Log::severityToString(Log::Fatal), "danger");
  _htmlWarningLogView->setTrClass("%1", 4, logTrClasses);
  _htmlWarningLogView->setItemDelegate(
        new HtmlLogEntryItemDelegate(_htmlWarningLogView));
  _wuiHandler->addView(_htmlWarningLogView);
  _htmlWarningLogView10 = new HtmlTableView(
        this, "warninglog10", SHORT_LOG_MAXROWS, SHORT_LOG_ROWSPERPAGE);
  _htmlWarningLogView10->setModel(_warningLogModel);
  _htmlWarningLogView10->setEllipsePlaceholder("(see log page for more entries)");
  _htmlWarningLogView10->setEmptyPlaceholder("(empty log)");
  _htmlWarningLogView10->setTrClass("%1", 4, logTrClasses);
  _htmlWarningLogView10->setItemDelegate(
        new HtmlLogEntryItemDelegate(_htmlWarningLogView10));
  _wuiHandler->addView(_htmlWarningLogView10);
  _htmlInfoLogView = new HtmlTableView(this, "infolog");
  _htmlInfoLogView->setModel(_infoLogModel);
  _htmlInfoLogView->setTrClass("%1", 4, logTrClasses);
  _htmlInfoLogView->setItemDelegate(
        new HtmlLogEntryItemDelegate(_htmlInfoLogView));
  _wuiHandler->addView(_htmlInfoLogView);
  _htmlAuditLogView = new HtmlTableView(this, "auditlog");
  _htmlAuditLogView->setModel(_auditLogModel);
  _htmlAuditLogView->setTrClass("%1", 4, logTrClasses);
  HtmlLogEntryItemDelegate *htmlLogEntryItemDelegate =
      new HtmlLogEntryItemDelegate(_htmlAuditLogView);
  htmlLogEntryItemDelegate->setMaxCellContentLength(2000);
  _htmlAuditLogView->setItemDelegate(htmlLogEntryItemDelegate);
  _wuiHandler->addView(_htmlAuditLogView);
  _htmlWarningLogView->setEmptyPlaceholder("(empty log)");
  _htmlUnfinishedTaskInstancesView =
      new HtmlTableView(this, "unfinishedtaskinstances",
                        _unfinishedTaskInstancesModel->maxrows(), 200);
  _htmlUnfinishedTaskInstancesView->setModel(_unfinishedTaskInstancesModel);
  QHash<QString,QString> taskInstancesTrClasses;
  taskInstancesTrClasses.insert("failure", "danger");
  taskInstancesTrClasses.insert("planned", "info");
  taskInstancesTrClasses.insert("queued", "warning");
  taskInstancesTrClasses.insert("running", "success");
  taskInstancesTrClasses.insert("waiting", "success");
  taskInstancesTrClasses.insert("canceled", "active");
  _htmlUnfinishedTaskInstancesView->setTrClass("%1", 2, taskInstancesTrClasses);
  _htmlUnfinishedTaskInstancesView->setEmptyPlaceholder("(no unfinished task)");
  _htmlUnfinishedTaskInstancesView->setColumnIndexes({0,1,2,3,4,15,17,18,8});
  _htmlUnfinishedTaskInstancesView
      ->setItemDelegate(new HtmlTaskInstanceItemDelegate(
                          _htmlUnfinishedTaskInstancesView));
  _wuiHandler->addView(_htmlUnfinishedTaskInstancesView);
  _htmlTaskInstancesView = new HtmlTableView(this, "taskinstances");
  _htmlTaskInstancesView->setModel(_taskInstancesHistoryModel);
  _htmlTaskInstancesView->setTrClass("%1", 2, taskInstancesTrClasses);
  _htmlTaskInstancesView->setEmptyPlaceholder("(no recent task instance)");
  _htmlTaskInstancesView->setColumnIndexes({0,10,1,2,3,4,12,16,6,14,8});
  _htmlTaskInstancesView
      ->setItemDelegate(new HtmlTaskInstanceItemDelegate(_htmlTaskInstancesView));
  _wuiHandler->addView(_htmlTaskInstancesView);
  _htmlHerdsView = new HtmlTableView(this, "herds", 20);
  _htmlHerdsView->setModel(_herdsHistoryModel);
  _htmlHerdsView->setTrClass("%1", 2, taskInstancesTrClasses);
  _htmlHerdsView->setEmptyPlaceholder("(no recent herd)");
  _htmlHerdsView->setColumnIndexes({10,1,2,3,14,11,8});
  auto htmlHerdsDelegate = new HtmlTaskInstanceItemDelegate(
    _htmlHerdsView, true);
  htmlHerdsDelegate->setMaxCellContentLength(16384);
  _htmlHerdsView->setItemDelegate(htmlHerdsDelegate);
  _wuiHandler->addView(_htmlHerdsView);
  _htmlTasksScheduleView = new HtmlTableView(this, "tasksschedule");
  _htmlTasksScheduleView->setModel(_tasksModel);
  _htmlTasksScheduleView->setEmptyPlaceholder("(no task in configuration)");
  _htmlTasksScheduleView->setColumnIndexes({11,2,5,6,19,10,17,18});
  _htmlTasksScheduleView->enableRowAnchor(11);
  _htmlTasksScheduleView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksScheduleView));
  _wuiHandler->addView(_htmlTasksScheduleView);
  _htmlTasksConfigView = new HtmlTableView(this, "tasksconfig");
  _htmlTasksConfigView->setModel(_tasksModel);
  _htmlTasksConfigView->setEmptyPlaceholder("(no task in configuration)");
  _htmlTasksConfigView->setColumnIndexes({1,0,3,5,4,28,8,12,18});
  _htmlTasksConfigView->enableRowAnchor(11);
  _htmlTasksConfigView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksConfigView));
  _wuiHandler->addView(_htmlTasksConfigView);
  _htmlTasksParamsView = new HtmlTableView(this, "tasksparams");
  _htmlTasksParamsView->setModel(_tasksModel);
  _htmlTasksParamsView->setEmptyPlaceholder("(no task in configuration)");
  _htmlTasksParamsView->setColumnIndexes({1,0,20,7,25,21,22,18});
  _htmlTasksParamsView->enableRowAnchor(11);
  _htmlTasksParamsView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksParamsView));
  _wuiHandler->addView(_htmlTasksParamsView);
  _htmlTasksListView = // note that this view is only used in /rest urls
      new HtmlTableView(this, "taskslist");
  _htmlTasksListView->setModel(_tasksModel);
  _htmlTasksListView->setEmptyPlaceholder("(no task in configuration)");
  _htmlTasksListView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksListView));
  _htmlTasksEventsView = new HtmlTableView(this, "tasksevents");
  _htmlTasksEventsView->setModel(_tasksModel);
  _htmlTasksEventsView->setEmptyPlaceholder("(no task in configuration)");
  _htmlTasksEventsView->setColumnIndexes({11,6,14,15,16,18});
  _htmlTasksEventsView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksEventsView));
  _wuiHandler->addView(_htmlTasksEventsView);
  _htmlSchedulerEventsView = new HtmlTableView(this, "schedulerevents");
  qobject_cast<HtmlItemDelegate*>(_htmlSchedulerEventsView->itemDelegate())
      ->setPrefixForColumnHeader(0, "<i class=\"fa-solid fa-play\"></i>&nbsp;")
      ->setPrefixForColumnHeader(1, "<i class=\"fa-solid fa-arrows-rotate\"></i>&nbsp;")
      ->setPrefixForColumnHeader(2, "<i class=\"fa-solid fa-comment\"></i>&nbsp;")
      ->setPrefixForColumnHeader(3, "<i class=\"fa-solid fa-file-lines\"></i>&nbsp;")
      ->setPrefixForColumnHeader(4, "<i class=\"fa-solid fa-play\"></i>&nbsp;")
      ->setPrefixForColumnHeader(5, "<i class=\"fa-solid fa-check\"></i>&nbsp;")
      ->setPrefixForColumnHeader(6, "<i class=\"fa-solid fa-circle-minus\"></i>&nbsp;")
      ->setMaxCellContentLength(2000);
  _htmlSchedulerEventsView->setModel(_schedulerEventsModel);
  _wuiHandler->addView(_htmlSchedulerEventsView);
  _htmlLastPostedNoticesView20 =
      new HtmlTableView(this, "lastpostednotices20",
                        _lastPostedNoticesModel->maxrows(), 20);
  _htmlLastPostedNoticesView20->setModel(_lastPostedNoticesModel);
  _htmlLastPostedNoticesView20->setEmptyPlaceholder("(no notice)");
  _htmlLastPostedNoticesView20->setColumnIndexes({0,1});
  qobject_cast<HtmlItemDelegate*>(_htmlLastPostedNoticesView20->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"fa-solid fa-comment\"></i>&nbsp;");
  _wuiHandler->addView(_htmlLastPostedNoticesView20);
  _htmlTaskGroupsView = new HtmlTableView(this, "taskgroups");
  _htmlTaskGroupsView->setModel(_sortedTaskGroupsModel);
  _htmlTaskGroupsView->setEmptyPlaceholder("(no task group)");
  _htmlTaskGroupsView->setColumnIndexes({0,2,7,21,22});
  qobject_cast<HtmlItemDelegate*>(_htmlTaskGroupsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa-solid fa-gears\"></i>&nbsp;");
  _wuiHandler->addView(_htmlTaskGroupsView);
  _htmlTaskGroupsEventsView = new HtmlTableView(this, "taskgroupsevents");
  _htmlTaskGroupsEventsView->setModel(_sortedTaskGroupsModel);
  _htmlTaskGroupsEventsView->setEmptyPlaceholder("(no task group)");
  _htmlTaskGroupsEventsView->setColumnIndexes({0,14,15,16});
  qobject_cast<HtmlItemDelegate*>(_htmlTaskGroupsEventsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"fa-solid fa-gears\"></i>&nbsp;")
      ->setPrefixForColumnHeader(14, "<i class=\"fa-solid fa-play\"></i>&nbsp;")
      ->setPrefixForColumnHeader(15, "<i class=\"fa-solid fa-check\"></i>&nbsp;")
      ->setPrefixForColumnHeader(16, "<i class=\"fa-solid fa-circle-minus\"></i>&nbsp;");
  _wuiHandler->addView(_htmlTaskGroupsEventsView);
  _htmlAlertChannelsView = new HtmlTableView(this, "alertchannels");
  _htmlAlertChannelsView->setModel(_alertChannelsModel);
  _htmlAlertChannelsView->enableRowHeaders();
  _wuiHandler->addView(_htmlAlertChannelsView);
  _htmlTasksResourcesView = new HtmlTableView(this, "tasksresources");
  _htmlTasksResourcesView->setModel(_tasksModel);
  _htmlTasksResourcesView->setEmptyPlaceholder("(no task)");
  _htmlTasksResourcesView->setColumnIndexes({11,17,8});
  _htmlTasksResourcesView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksResourcesView));
  _wuiHandler->addView(_htmlTasksResourcesView);
  _htmlTasksAlertsView = new HtmlTableView(this, "tasksalerts");
  _htmlTasksAlertsView->setModel(_tasksModel);
  _htmlTasksAlertsView->setEmptyPlaceholder("(no task)");
  _htmlTasksAlertsView->setColumnIndexes({11,6,23,26,24,27,12,16,18});
  _htmlTasksAlertsView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksAlertsView));
  _wuiHandler->addView(_htmlTasksAlertsView);
  _htmlLogFilesView = new HtmlTableView(this, "logfiles");
  _htmlLogFilesView->setModel(_logConfigurationModel);
  _htmlLogFilesView->setEmptyPlaceholder("(no log file)");
  QHash<QString,QString> bufferLogFileIcons;
  bufferLogFileIcons.insert("true", "<i class=\"fa-solid fa-download\"></i>&nbsp;");
  qobject_cast<HtmlItemDelegate*>(_htmlLogFilesView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"fa-solid fa-file-lines\"></i>&nbsp;")
      ->setPrefixForColumn(3, "%1", 3, bufferLogFileIcons);
  _htmlLogFilesView->setColumnIndexes({1,2,3});
  _wuiHandler->addView(_htmlLogFilesView);
  _htmlCalendarsView = new HtmlTableView(this, "calendars");
  _htmlCalendarsView->setModel(_sortedCalendarsModel);
  _htmlCalendarsView->setColumnIndexes({1,2});
  _htmlCalendarsView->setEmptyPlaceholder("(no named calendar)");
  qobject_cast<HtmlItemDelegate*>(_htmlCalendarsView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"fa-solid fa-calendar-days\"></i>&nbsp;");
  _wuiHandler->addView(_htmlCalendarsView);
  _htmlConfigsView = new HtmlTableView(this, "configs");
  _htmlConfigsView->setModel(_configsModel);
  _htmlConfigsView->setEmptyPlaceholder("(no config)");
  _htmlConfigsDelegate =
      new HtmlSchedulerConfigItemDelegate(0, 2, 3, _htmlConfigsView);
  _htmlConfigsView->setItemDelegate(_htmlConfigsDelegate);
  _wuiHandler->addView(_htmlConfigsView);
  _htmlConfigHistoryView = new HtmlTableView(this, "confighistory");
  _htmlConfigHistoryView->setModel(_configHistoryModel);
  _htmlConfigHistoryView->setEmptyPlaceholder("(empty history)");
  _htmlConfigHistoryView->setColumnIndexes({1,2,3,4});
  _htmlConfigHistoryDelegate =
      new HtmlSchedulerConfigItemDelegate(3, -1, 4, _htmlConfigsView);
  _htmlConfigHistoryView->setItemDelegate(_htmlConfigHistoryDelegate);
  _wuiHandler->addView(_htmlConfigHistoryView);

  // CSV views
  CsvFormatter::setDefaultFieldQuote('"'); // LATER remove if CsvTableView is no longer used
  CsvFormatter::setDefaultReplacementChar(' '); // same
  _csvFreeResourcesView = new CsvTableView(this);
  _csvFreeResourcesView->setModel(_freeResourcesModel);
  _csvFreeResourcesView->enableRowHeaders();
  _csvResourcesLwmView = new CsvTableView(this);
  _csvResourcesLwmView->setModel(_resourcesLwmModel);
  _csvResourcesLwmView->enableRowHeaders();
  _csvResourcesConsumptionView = new CsvTableView(this);
  _csvResourcesConsumptionView->setModel(_resourcesConsumptionModel);
  _csvResourcesConsumptionView->enableRowHeaders();
  _csvGlobalParamsView = new CsvTableView(this);
  _csvGlobalParamsView->setModel(_globalParamsModel);
  _csvGlobalVarsView = new CsvTableView(this);
  _csvGlobalVarsView->setModel(_globalVarsModel);
  _csvAlertParamsView = new CsvTableView(this);
  _csvAlertParamsView->setModel(_alertParamsModel);
  _csvStatefulAlertsView = new CsvTableView(this);
  _csvStatefulAlertsView->setModel(_sortedStatefulAlertsModel);
  _csvLastEmittedAlertsView =
      new CsvTableView(this, QString(), _lastEmittedAlertsModel->maxrows());
  _csvLastEmittedAlertsView->setModel(_lastEmittedAlertsModel);
  _csvGridboardsView = new CsvTableView(this);
  _csvGridboardsView->setModel(_sortedGridboardsModel);
  _csvTaskInstancesView =
      new CsvTableView(this, QString(), _taskInstancesHistoryModel->maxrows());
  _csvTaskInstancesView->setModel(_taskInstancesHistoryModel);
  _csvSchedulerEventsView = new CsvTableView(this);
  _csvSchedulerEventsView->setModel(_schedulerEventsModel);
  _csvLastPostedNoticesView =
      new CsvTableView(this, QString(), _lastPostedNoticesModel->maxrows());
  _csvLastPostedNoticesView->setModel(_lastPostedNoticesModel);
  _csvConfigsView = new CsvTableView(this);
  _csvConfigsView->setModel(_configsModel);
  _csvConfigHistoryView = new CsvTableView(this);
  _csvConfigHistoryView->setModel(_configHistoryModel);

  // dedicated thread
  _thread->setObjectName("WebConsoleServer");
  connect(this, &WebConsole::destroyed, _thread, &QThread::quit);
  connect(_thread, &QThread::finished, _thread, &QThread::deleteLater);
  _thread->start();
  moveToThread(_thread);

  // unparenting models that must not be deleted automaticaly when deleting
  // this
  // note that they must be children until there to be moved to the right thread
  /* LATER with 3 following lines, works on linux Qt 5.4.2 64bits but not on Win Qt 5.5.1 32bits
ASSERT failure in QCoreApplication::sendEvent: "Cannot send events to objects
 owned by a different thread. Current thread d227b98.
 Receiver '' (of type 'WebConsole') was created in thread d233478",
 file kernel\qcoreapplication.cpp, line 553
This application has requested the Runtime to terminate it in an unusual way.
Please contact the application's support team for more information.
QWaitCondition: Destroyed while threads are still waiting
   */
#ifndef Q_OS_WIN
  _warningLogModel->setParent(0);
  _infoLogModel->setParent(0);
  _auditLogModel->setParent(0);
#endif
  // LATER find a safe way to delete logmodels asynchronously without crashing log framework
}

WebConsole::~WebConsole() {
}

bool WebConsole::acceptRequest(HttpRequest req) {
  Q_UNUSED(req)
  return true;
}

static RadixTree<std::function<QVariant(const WebConsole *, const QString &key)>
                 > _serverStats {
{ "scheduler.startdate", [](const WebConsole *console, const QString &) {
  return console->scheduler()->startdate().toUTC();
} },
{ "scheduler.uptime", [](const WebConsole *console, const QString &) {
  return (double)console->scheduler()->startdate().msecsTo(QDateTime::currentDateTime())*.001;
} },
{ "scheduler.configdate", [](const WebConsole *console, const QString &) {
  return console->scheduler()->configdate().toUTC();
} },
{ "scheduler.execcount", [](const WebConsole *console, const QString &) {
  return console->scheduler()->execCount();
} },
{ "scheduler.runningtaskshwm", [](const WebConsole *console, const QString &) {
  return console->scheduler()->runningTasksHwm();
} },
{ "scheduler.queuedtaskshwm", [](const WebConsole *console, const QString &) {
  return console->scheduler()->queuedTasksHwm();
} },
{ "scheduler.taskscount", [](const WebConsole *console, const QString &) {
  return console->scheduler()->tasksCount();
} },
{ "scheduler.taskgroupscount", [](const WebConsole *console, const QString &) {
  return console->scheduler()->taskGroupsCount();
} },
{ "scheduler.maxtotaltaskinstances", [](const WebConsole *console, const QString &) {
  return console->scheduler()->maxtotaltaskinstances();
} },
{ "scheduler.maxqueuedrequests", [](const WebConsole *console, const QString &) {
  return console->scheduler()->maxqueuedrequests();
} },
{ "alerter.rulescachesize", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->rulesCacheSize();
} },
{ "alerter.rulescachehwm", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->rulesCacheHwm();
} },
{ "alerter.alertsettingscount", [](const WebConsole *console, const QString &) {
  return console->alerterConfig().alertSettings().size();
} },
{ "alerter.alertsubscriptionscount", [](const WebConsole *console, const QString &) {
  return console->alerterConfig().alertSubscriptions().size();
} },
{ "alerter.raiserequestscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->raiseRequestsCounter();
} },
{ "alerter.raiseimmediaterequestscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->raiseImmediateRequestsCounter();
} },
{ "alerter.raisenotificationscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->raiseNotificationsCounter();
} },
{ "alerter.cancelrequestscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->cancelRequestsCounter();
} },
{ "alerter.cancelimmediaterequestscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->cancelImmediateRequestsCounter();
} },
{ "alerter.cancelnotificationscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->cancelNotificationsCounter();
} },
{ "alerter.emitrequestscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->emitRequestsCounter();
} },
{ "alerter.emitnotificationscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->emitNotificationsCounter();
} },
{ "alerter.totalchannelsnotificationscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->totalChannelsNotificationsCounter();
} },
{ "alerter.deduplicatingalertscount", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->deduplicatingAlertsCount();
} },
{ "alerter.deduplicatingalertshwm", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->deduplicatingAlertsHwm();
} },
{ "gridboards.evaluationscounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->gridboardsEvaluationsCounter();
} },
{ "gridboards.updatescounter", [](const WebConsole *console, const QString &) {
  return console->scheduler()->alerter()->gridboardsUpdatesCounter();
} },
{ "configrepository.configfilepath", [](const WebConsole *console, const QString &) {
  return console->configFilePath();
} },
{ "configrepository.configrepopath", [](const WebConsole *console, const QString &) {
  return console->configRepoPath();
} },
};

class ServerStatsProvider : public ParamsProvider {
  WebConsole *_console;

public:
  ServerStatsProvider(WebConsole *console) : _console(console) { }
  QVariant paramValue(QString key, const ParamsProvider *context,
                      QVariant defaultValue, QSet<QString>) const override {
    Q_UNUSED(context)
    if (!_console || !_console->_scheduler) // should never happen
      return defaultValue;
    auto handler = _serverStats.value(key);
    return handler ? handler(_console, key) : defaultValue;
  }
  QSet<QString> keys() const override {
    return _serverStats.keys();
  }
};

inline bool enforceMethods(int methodsMask, HttpRequest req,
                             HttpResponse res) {
  if (req.method() & methodsMask)
    return true;
  res.setStatus(HttpResponse::HTTP_Method_Not_Allowed);
  res.output()->write("Method not allowed for this resource.");
  return false;
}

inline bool writeHtmlView(HtmlTableView *view, HttpRequest req,
                          HttpResponse res) {
  QByteArray data = view->text().toUtf8();
  res.setContentType("text/html;charset=UTF-8");
  res.setContentLength(data.size());
  if (req.method() != HttpRequest::HEAD)
    res.output()->write(data);
  return true;
}

inline bool writeCsvView(CsvTableView *view, HttpRequest req,
                         HttpResponse res) {
  // LATER write on the fly past a certain size
  QByteArray data = view->text().toUtf8();
  res.setContentType("text/csv;charset=UTF-8");
  res.setHeader("Content-Disposition", "attachment"); // LATER filename=table.csv");
  res.setContentLength(data.size());
  if (req.method() != HttpRequest::HEAD)
    res.output()->write(data);
  return true;
}

inline bool writeItemsAsCsv(
    SharedUiItemList<> list, HttpRequest req, HttpResponse res) {
  // LATER write on the fly past a certain size
  QByteArray data = _csvFormatter.formatTable(list).toUtf8();
  res.setContentType("text/csv;charset=UTF-8");
  res.setHeader("Content-Disposition", "attachment"); // LATER filename=table.csv");
  res.setContentLength(data.size());
  if (req.method() != HttpRequest::HEAD)
    res.output()->write(data);
  return true;
}

template <class T>
inline bool writeItemsAsCsv(
    QList<T> list, HttpRequest req, HttpResponse res) {
  return writeItemsAsCsv(SharedUiItemList<>(SharedUiItemList<T>(list)),
                         req, res);
}

inline bool sortAndWriteItemsAsCsv(
    SharedUiItemList<> list, HttpRequest req, HttpResponse res) {
  std::sort(list.begin(), list.end());
  return writeItemsAsCsv(list, req, res);
}

template <class T>
inline bool sortAndWriteItemsAsCsv(
    QList<T> list, HttpRequest req, HttpResponse res) {
  return sortAndWriteItemsAsCsv(SharedUiItemList<>(SharedUiItemList<T>(list)),
                                req, res);
}

inline bool writeItemsAsHtmlTable(
    SharedUiItemList<> list, HttpRequest req, HttpResponse res) {
  QByteArray data = _htmlTableFormatter.formatTable(list).toUtf8();
  res.setContentType("text/html;charset=UTF-8");
  res.setContentLength(data.size());
  if (req.method() != HttpRequest::HEAD)
    res.output()->write(data);
  return true;
}

template <class T>
inline bool writeItemsAsHtmlTable(
    QList<T> list, HttpRequest req, HttpResponse res) {
  return writeItemsAsHtmlTable(SharedUiItemList<>(SharedUiItemList<T>(list)),
                         req, res);
}

inline bool sortAndWriteItemsAsHtmlTable(
    SharedUiItemList<> list, HttpRequest req, HttpResponse res) {
  std::sort(list.begin(), list.end());
  return writeItemsAsHtmlTable(list, req, res);
}

template <class T>
inline bool sortAndWriteItemsAsHtmlTable(
    QList<T> list, HttpRequest req, HttpResponse res) {
  return sortAndWriteItemsAsHtmlTable(
        SharedUiItemList<>(SharedUiItemList<T>(list)), req, res);
}

static bool writeSvgImage(QByteArray data, HttpRequest req,
                          HttpResponse res) {
  res.setContentType("image/svg+xml");
  res.setContentLength(data.size());
  if (req.method() != HttpRequest::HEAD)
    res.output()->write(data);
  return true;
}

static bool writePlainText(QByteArray data, HttpRequest req,
                           HttpResponse res,
                           QString contentType = "text/plain;charset=UTF-8") {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
    return true;
  res.setContentType(contentType);
  res.setContentLength(data.size());
  if (req.method() != HttpRequest::HEAD)
    res.output()->write(data);
  return true;
}

static void copyFilteredFiles(QStringList paths, QIODevice *output,
                              QString pattern, bool useRegexp) {
  // LATER handle HEAD differently and write Content-Length header
  for (const QString &path: paths) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
      if (pattern.isEmpty())
        IOUtils::copy(output, &file);
      else if (useRegexp)
        IOUtils::grepWithContinuation(
              output, &file, QRegularExpression(pattern), "  ");
      else
        IOUtils::grepWithContinuation(output, &file, pattern, "  ");
    } else {
      Log::warning() << "web console cannot open log file " << path
                     << " : error #" << file.error() << " : "
                     << file.errorString();
    }
  }
}

static const QList<quint64> _noAuditInstanceIds { 0 };

static void apiAuditAndResponse(
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, QString responseMessage,
    QString auditAction, QString auditTaskId = QString(),
    QList<quint64> auditInstanceIds = _noAuditInstanceIds) {
  if (auditInstanceIds.isEmpty()) // should never happen
    auditInstanceIds = _noAuditInstanceIds;
  QString userid = processingContext->paramValue(
        QStringLiteral("userid")).toString();
  QString referer = req.header(QStringLiteral("Referer"));
  QString redirect = req.base64Cookie("redirect", referer);
  bool disableRedirect = req.header("Prefer")
      .contains("return=representation", Qt::CaseInsensitive);
  if (auditAction.contains(webconsole->showAuditEvent()) // empty regexps match any string
      && (webconsole->hideAuditEvent().pattern().isEmpty()
          || !auditAction.contains(webconsole->hideAuditEvent()))
      && userid.contains(webconsole->showAuditUser())
      && (webconsole->hideAuditUser().pattern().isEmpty()
          || !userid.contains(webconsole->hideAuditUser()))) {
    for (quint64 auditInstanceId: auditInstanceIds)
      Log::info(auditTaskId, auditInstanceId)
          << "AUDIT action: '" << auditAction
          << (responseMessage.startsWith('E')
              ? "' result: failure" : "' result: success")
          << " actor: '" << userid
          << "' address: { " << req.clientAdresses().join(", ")
          << " } params: " << req.paramsAsParamSet().toString(false)
          << " response message: " << responseMessage;
  }
  if (!disableRedirect && !redirect.isEmpty()) {
    res.setBase64SessionCookie(QStringLiteral("message"), responseMessage,
                               QStringLiteral("/"));
    res.clearCookie(QStringLiteral("redirect"), QStringLiteral("/"));
    res.redirect(redirect);
  } else {
    res.setContentType(QStringLiteral("text/plain;charset=UTF-8"));
    if (res.status() == HttpResponse::HTTP_Ok // won't override if already set
        && responseMessage.startsWith('E'))
      res.setStatus(HttpResponse::HTTP_Internal_Server_Error);
    responseMessage.append('\n');
    res.output()->write(responseMessage.toUtf8());
  }
}

// syntaxic sugar for 1-instance-may-even-be-null cases
static void apiAuditAndResponse(
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, QString responseMessage,
    QString auditAction, TaskInstance instance) {
  if (instance.isNull())
    apiAuditAndResponse(webconsole, req, res, processingContext,
                        responseMessage, auditAction);
  else
    apiAuditAndResponse(webconsole, req, res, processingContext,
                        responseMessage, auditAction, instance.task().id(),
                        { instance.idAsLong() });
}

static RadixTree<
std::function<bool(WebConsole *, HttpRequest, HttpResponse,
                   ParamsProviderMerger *, int matchedLength)>> _handlers {
{ "/do/v1/tasks/request/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString taskId = req.url().path().mid(matchedLength);
  ParamSet params = req.paramsAsParamSet();
  // LATER drop parameters that are not defined as overridable in task config
  // remove empty values so that fields left empty in task request ui form won't
  // override configurated values
  for (const QString &key: params.keys())
    if (params.rawValue(key).isEmpty())
      params.removeValue(key);
  // LATER should check that mandatory form fields have been set ?
  TaskInstanceList instances = webconsole->scheduler()
      ->requestTask(taskId, params, params.valueAsBool("force", false),
                    params.value("herdid", "0").toULongLong());
  QString message;
  QList<quint64> taskInstanceIds;
  if (!instances.isEmpty()) {
    message = "S:Task '"+taskId+"' submitted for execution with id";
    for (const TaskInstance &request: instances) {
      message.append(' ').append(QString::number(request.idAsLong()));
      taskInstanceIds << request.idAsLong();
    }
    message.append('.');
  } else
    message = "E:Execution request of task '"+taskId
        +"' failed (see logs for more information).";
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      taskId, instances);
  return true;
}, true },
{ "/do/v1/tasks/abort_instances/", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString taskId = req.url().path().mid(matchedLength);
  QString message;
  TaskInstanceList instances =
      webconsole->scheduler()->abortTaskInstanceByTaskId(taskId);
  message = "S:Task instances { "+instances.join(' ')+" } aborted.";
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      taskId, instances);
  return true;
}, true },
{ "/do/v1/tasks/cancel_requests/", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString taskId = req.url().path().mid(matchedLength);
  QString message;
  TaskInstanceList instances =
      webconsole->scheduler()->cancelTaskInstancesByTaskId(taskId);
  message = "S:Task requests { "+instances.join(' ')+" } canceled.";
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      taskId, instances);
  return true;
}, true },
{ "/do/v1/tasks/cancel_requests_and_abort_instances/", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString taskId = req.url().path().mid(matchedLength);
  QString message;
  TaskInstanceList instances =
      webconsole->scheduler()->cancelTaskInstancesByTaskId(taskId);
  message = "S:Task requests { "+instances.join(' ')
      +" } canceled and task instances { ";
  instances = webconsole->scheduler()->abortTaskInstanceByTaskId(taskId);
  message += instances.join(' ')+" } aborted.";
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      taskId, instances);
  return true;
}, true },
{ "/do/v1/tasks/enable_all", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString taskId = req.url().path().mid(matchedLength);
  QString message;
  webconsole->scheduler()->enableAllTasks(true);
  message = "S:Enabled all tasks.";
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      taskId);
  // wait to make it less probable that the page displays before effect
  QThread::usleep(500000);
  return true;
} },
{ "/do/v1/tasks/disable_all", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString taskId = req.url().path().mid(matchedLength);
  QString message;
  webconsole->scheduler()->enableAllTasks(false);
  message = "S:Disabled all tasks.";
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      taskId);
  // wait to make it less probable that the page displays before effect
  QThread::usleep(500000);
  return true;
} },
{ "/do/v1/tasks/enable/", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString taskId = req.url().path().mid(matchedLength);
  QString message;
  if (webconsole->scheduler()->enableTask(taskId, true))
    message = "S:Task '"+taskId+"' enabled.";
  else {
    message = "E:Task '"+taskId+"' not found.";
    res.setStatus(HttpResponse::HTTP_Not_Found);
  }
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      taskId);
  return true;
}, true },
{ "/do/v1/tasks/disable/", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString taskId = req.url().path().mid(matchedLength);
  QString message;
  if (webconsole->scheduler()->enableTask(taskId, false))
    message = "S:Task '"+taskId+"' disabled.";
  else {
    message = "E:Task '"+taskId+"' not found.";
    res.setStatus(HttpResponse::HTTP_Not_Found);
  }
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      taskId);
  return true;
}, true },
{ "/do/v1/taskinstances/abort/", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  quint64 taskInstanceId = req.url().path().mid(matchedLength).toLongLong();
  QString message;
  TaskInstance instance = webconsole->scheduler()->abortTaskInstance(taskInstanceId);
  if (instance.isNull()) {
    message = "E:Failed to abort task instance "
        +QString::number(taskInstanceId)+".";
    res.setStatus(500);
  } else {
    message = "S:Task instance "+QString::number(taskInstanceId)+" aborted.";
  }
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/taskinstances/cancel/", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  quint64 taskInstanceId = req.url().path().mid(matchedLength).toLongLong();
  QString message;
  TaskInstance instance =
      webconsole->scheduler()->cancelTaskInstance(taskInstanceId);
  if (instance.isNull()) {
    message = "E:Failed to cancel task request "
        +QString::number(taskInstanceId)+".";
    res.setStatus(500);
  } else {
    message = "S:Task request "+QString::number(taskInstanceId)+" canceled.";
  }
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      instance);
  return true;
}, true },
{ "/do/v1/taskinstances/cancel_or_abort/", [](
WebConsole *webconsole, HttpRequest req, HttpResponse res,
ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  quint64 taskInstanceId = req.url().path().mid(matchedLength).toLongLong();
  QString message;
  TaskInstance instance =
      webconsole->scheduler()->cancelTaskInstance(taskInstanceId);
  if (instance.isNull()) {
    instance = webconsole->scheduler()->abortTaskInstance(taskInstanceId);
    if (instance.isNull()) {
      message = "E:Failed to cancel or abort task instance "
          +QString::number(taskInstanceId)+".";
      res.setStatus(500);
    } else {
      message = "S:Task instance "+QString::number(taskInstanceId)+" aborted.";
    }
  } else {
    message = "S:Task request "+QString::number(taskInstanceId)+" canceled.";
  }
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength),
                      instance);
  return true;
}, true },
{ "/do/v1/notices/post/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString notice = req.url().path().mid(matchedLength) || req.param("notice");
  ParamSet params = req.paramsAsParamSet();
  params.removeValue("notice");
  webconsole->scheduler()->postNotice(notice, params);
  QString message;
  message = "S:Notice '"+notice+"' posted.";
  apiAuditAndResponse(webconsole, req, res, processingContext, message,
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/alerts/raise/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString alertid = req.url().path().mid(matchedLength) || req.param("alertid");
  webconsole->scheduler()->alerter()->raiseAlert(alertid);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      "S:Raised alert '"+alertid+"'.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/alerts/raise_immediately/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString alertid = req.url().path().mid(matchedLength) || req.param("alertid");
  webconsole->scheduler()->alerter()->raiseAlertImmediately(alertid);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      "S:Raised alert '"+alertid+"' immediately.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/alerts/cancel/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString alertid = req.url().path().mid(matchedLength) || req.param("alertid");
  webconsole->scheduler()->alerter()->cancelAlert(alertid);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      "S:Canceled alert '"+alertid+"'.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/alerts/cancel_immediately/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString alertid = req.url().path().mid(matchedLength) || req.param("alertid");
  webconsole->scheduler()->alerter()->cancelAlertImmediately(alertid);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      "S:Canceled alert '"+alertid+"' immediately.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/alerts/emit/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString alertid = req.url().path().mid(matchedLength) || req.param("alertid");
  webconsole->scheduler()->alerter()->emitAlert(alertid);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      "S:Emitted alert '"+alertid+"'.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/gridboards/clear/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString gridboardid = req.url().path().mid(matchedLength)
      || req.param("gridboardid");
  webconsole->scheduler()->alerter()->clearGridboard(gridboardid);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      "S:Gridboard '"+gridboardid+"' cleared.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/scheduler/shutdown", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  Qrond::instance()->asyncShutdown(0);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      "S:Shutdown requested.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },

{ "/do/v1/configs/reload_config_file", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  bool ok = Qrond::instance()->loadConfig();
  if (!ok)
    res.setStatus(HttpResponse::HTTP_Internal_Server_Error);
  else
    // wait to make it less probable that the page displays before effect
    QThread::usleep(1000000);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      ok ? "S:Configuration reloaded."
                         : "E:Cannot reload configuration.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/configs/activate/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString configid = req.url().path().mid(matchedLength)
      || req.param("configid");
  bool ok = webconsole->configRepository()->activateConfig(configid);
  if (!ok)
    res.setStatus(HttpResponse::HTTP_Internal_Server_Error);
  else
    // wait to make it less probable that the page displays before effect
    QThread::usleep(1000000);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      ok ? "S:Configuration '"+configid+"' activated."
                         : "E:Cannot activate configuration '"+configid+"'.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/do/v1/configs/remove/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::POST, req, res))
    return true;
  QString configid = req.url().path().mid(matchedLength)
      || req.param("configid");
  bool ok = webconsole->configRepository()->removeConfig(configid);
  if (!ok)
    res.setStatus(HttpResponse::HTTP_Internal_Server_Error);
  else
    // wait to make it less probable that the page displays before effect
    QThread::usleep(1000000);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      ok ? "S:Configuration '"+configid+"' removed."
                         : "E:Cannot remove configuration '"+configid+"'.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ "/console/confirm/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD|HttpRequest::POST,
                          req, res))
        return true;
      QString path = req.url().path().mid(matchedLength);
      QString message = req.param("confirm_message") || path;
      if (path.isEmpty()) {
        res.setStatus(HttpResponse::HTTP_Internal_Server_Error);
        res.output()->write("Confirmation page error.\n");
        return true;
      }
      path = processingContext->paramString("!pathtoroot")+"../"+path;
      QString referer = req.header(
            "Referer", processingContext->paramString("!pathtoroot"));
      QUrlQuery query(req.url());
      query.removeAllQueryItems("confirm_message");
      QString queryString = query.toString(QUrl::FullyEncoded);
      message = "<div class=\"well\">"
                "<h4 class=\"text-center\">Are you sure you want to "+message
          +" ?</h4><p><p class=\"text-center\"><a class=\"btn btn-danger\" "
           "href=\""+path;
      if (!queryString.isEmpty())
        message = message+"?"+queryString;
      message = message+"\">Yes, sure</a> <a class=\"btn\" href=\""+referer
          +"\">Cancel</a></div>";
      res.setBase64SessionCookie("redirect", referer, "/");
      QUrl url(req.url());
      url.setPath("/console/adhoc.html");
      url.setQuery(QString());
      req.overrideUrl(url);
      ParamsProviderMergerRestorer restorer(processingContext);
      processingContext->overrideParamValue("content", message);
      res.clearCookie("message", "/");
      webconsole->wuiHandler()->handleRequest(req, res, processingContext);
      return true;
}, true },
{ "/console/tasks/request/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::POST|HttpRequest::HEAD,
                          req, res))
        return true;
      QString taskId = req.url().path().mid(matchedLength);
      QString referer = req.header(
            "Referer", processingContext->paramString("!pathtoroot"));
      Task task(webconsole->scheduler()->task(taskId));
      if (!task.isNull()) {
        QUrl url(req.url());
        // LATER requestform.html instead of adhoc.html, after finding a way to handle foreach loop
        url.setPath("/console/adhoc.html");
        req.overrideUrl(url);
        QString form = "<div class=\"well\">\n"
                       "<h4 class=\"text-center\">About to start task "+taskId+"</h4>\n";
        if (task.label() != task.id())
          form +="<h4 class=\"text-center\">("+task.label()+")</h4>\n";
        form += "<p>\n";
        if (!task.requestFormFields().isEmpty())
          form += "<p class=\"text-center\">Task parameters can be defined in "
                  "the following form:";
        form += "<p><form action=\"../../../do/v1/tasks/request/"+taskId
            +"\" method=\"POST\">";
        bool errorOccured = false;
        for (const RequestFormField &rff: task.requestFormFields())
            form.append(rff.toHtmlFormFragment(
                            webconsole->readOnlyResourcesCache(),
                            &errorOccured));
        form += "<div><p><p class=\"text-center\">"
                "<button type=\"submit\" class=\"btn ";
        if (errorOccured)
            form += "btn-default\" disabled><i class=\"fa-solid fa-ban\"></i> Cannot"
                    " request task execution";
        else
            form += "btn-danger\">Request task execution";
        form += "</button>\n"
                "<a class=\"btn\" href=\""+referer+"\">Cancel</a></div>\n"
                "</form>\n"
                "</div>\n";
        ParamsProviderMergerRestorer restorer(processingContext);
        processingContext->overrideParamValue("content", form);
        res.setBase64SessionCookie("redirect", referer, "/");
        res.clearCookie("message", "/");
        webconsole->wuiHandler()->handleRequest(req, res, processingContext);
        return true;
      }
      res.setBase64SessionCookie("message", "E:Task '"+taskId+"' not found.",
                                 "/");
      res.redirect(referer);
      return true;
}, true },
{ "/console/tasks/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      CharacterSeparatedExpression elements(req.url().path(), matchedLength-1);
      QString taskId = elements.value(0);
      QString referer = req.header(
            "Referer", processingContext->paramString("!pathtoroot"));
      Task task(webconsole->scheduler()->task(taskId));
      if (task.isNull()) {
        if (referer.isEmpty()) {
          res.setStatus(404);
          res.output()->write("Task not found.");
        } else {
          res.setBase64SessionCookie("message", "E:Task '"+taskId+"' not found.",
                                     "/");
          res.redirect(referer);
        }
        return true;
      }
      ParamsProviderMergerRestorer restorer(processingContext);
      processingContext->overrideParamValue(
            "pfconfig",
            ParamSet::escape(
              QString::fromUtf8(
                task.originalPfNode().toPf(PfOptions().setShouldIndent()
                                     .setShouldWriteContentBeforeSubnodes()))));
      TaskPseudoParamsProvider tppp = task.pseudoParams();
      SharedUiItemParamsProvider itemAsParams(task);
      processingContext->prepend(&tppp);
      processingContext->prepend(&itemAsParams);
      res.clearCookie("message", "/");
      QUrl url(req.url());
      url.setPath("/console/task.html");
      url.setQuery(QString());
      req.overrideUrl(url);
      webconsole->wuiHandler()->handleRequest(req, res, processingContext);
      return true;
}, true },
{ "/rest/v1/tasks/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      CharacterSeparatedExpression elements(req.url().path(), matchedLength-1);
      QString taskId = elements.value(0);
      QString subItem = elements.value(1);
      Task task(webconsole->scheduler()->task(taskId));
      if (task.isNull()) {
        res.setStatus(404);
        res.output()->write("Task not found.");
        return true;
      }
      if (subItem == "config.pf") {
        return writePlainText(task.originalPfNode().toPf(
                                PfOptions().setShouldIndent()
                                .setShouldWriteContentBeforeSubnodes()),
                              req, res);
      }
      if (subItem == "list.csv") {
        return writeItemsAsCsv(task, req, res);
      }
      return false;
}, true },
{ "/console/gridboards/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::POST|HttpRequest::HEAD,
                          req, res))
        return true;
      QString gridboardId = req.url().path().mid(matchedLength);
      QString referer = req.header(
            "Referer", processingContext->paramString("!pathtoroot"));
      Gridboard gridboard(webconsole->scheduler()->alerter()
                          ->gridboard(gridboardId));
      if (!gridboard.isNull()) {
        SharedUiItemParamsProvider itemAsParams(gridboard);
        ParamsProviderMergerRestorer restorer(processingContext);
        processingContext->append(&itemAsParams);
        processingContext->overrideParamValue("gridboard.data",
                                              gridboard.toHtml());
        res.clearCookie("message", "/");
        QUrl url(req.url());
        url.setPath("/console/gridboard.html");
        url.setQuery(QString());
        req.overrideUrl(url);
        webconsole->wuiHandler()->handleRequest(req, res, processingContext);
      } else {
        res.setBase64SessionCookie("message", "E:Gridboard '"+gridboardId
                                   +"' not found.", "/");
        res.redirect(referer);
      }
      return true;
}, true },
{ "/console/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::POST|HttpRequest::HEAD,
                          req, res))
        return true;
      QList<QPair<QString,QString> > queryItems(req.urlQuery().queryItems());
      if (!queryItems.isEmpty()) {
        // if there are query parameters in url, transform them into cookies
        // it is useful for page changes on views
        // LATER this mechanism should be generic/framework (in libqtssu),
        // provided it does not applies to any resource (it's a common hack e.g.
        // to have query strings on font files)
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
        qsizetype i = s.lastIndexOf('/');
        if (i != -1)
          s = s.mid(i+1);
        if (!anchor.isEmpty())
          s.append('#').append(anchor);
        res.redirect(s);
      } else {
        SharedUiItemParamsProvider alerterConfigAsParams(
              webconsole->alerterConfig());
        ParamsProviderMergerRestorer restorer(processingContext);
        if (req.url().path().endsWith("/console/alerts.html")) {
          processingContext->append(&alerterConfigAsParams);
        }
        res.clearCookie("message", "/");
        webconsole->wuiHandler()->handleRequest(req, res, processingContext);
      }
      return true;
}, true },
{ { "/console/css/", "/console/js/", "/console/font/",
    "/console/img/" }, [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int) {
      // less processing for static resources than for general /console/
      if (!enforceMethods(HttpRequest::GET|HttpRequest::POST|HttpRequest::HEAD,
                          req, res))
        return true;
      res.clearCookie("message", "/");
      webconsole->wuiHandler()->handleRequest(req, res, processingContext);
      return true;
}, true },
{ "/rest/v1/taskgroups/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return sortAndWriteItemsAsCsv(
            webconsole->scheduler()->config().taskgroups().values(), req, res);
} },
{ "/rest/v1/taskgroups/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      // LATER handle fields comma enumeration for real
      QString fields = req.param(QStringLiteral("fields")).trimmed();
      if (fields == QStringLiteral("id,onstart,onsuccess,onfailure"))
        return writeHtmlView(webconsole->htmlTaskGroupsEventsView(), req, res);
      return writeHtmlView(webconsole->htmlTaskGroupsView(), req, res);
} },
{ "/rest/v1/tasks/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger*, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return sortAndWriteItemsAsCsv(
            webconsole->scheduler()->config().tasks().values(), req, res);
} },
{ "/rest/v1/tasks/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger*, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      // LATER handle fields comma enumeration for real
      QString fields = req.param(QStringLiteral("fields")).trimmed();
      if (fields == QStringLiteral("id,triggers,onstart,onsuccess,onfailure"))
        return writeHtmlView(webconsole->htmlTasksEventsView(), req, res);
      return writeHtmlView(webconsole->htmlTasksListView(), req, res);
} },
{ "/rest/v1/hosts/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return sortAndWriteItemsAsCsv(
            webconsole->scheduler()->config().hosts().values(), req, res);
} },
{ "/rest/v1/hosts/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlHostsListView(), req, res);
} },
{ "/rest/v1/clusters/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return sortAndWriteItemsAsCsv(
            webconsole->scheduler()->config().clusters().values(), req, res);
} },
{ "/rest/v1/clusters/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlClustersListView(), req, res);
} },
{ "/rest/v1/resources/free_resources_by_host.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvFreeResourcesView(), req, res);
} },
{ "/rest/v1/resources/free_resources_by_host.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlFreeResourcesView(), req, res);
} },
{ "/rest/v1/resources/lwm_resources_by_host.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvResourcesLwmView(), req, res);
} },
{ "/rest/v1/resources/lwm_resources_by_host.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlResourcesLwmView(), req, res);
} },
{ "/rest/v1/resources/consumption_matrix.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvResourcesConsumptionView(), req, res);
} },
{ "/rest/v1/resources/consumption_matrix.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlResourcesConsumptionView(), req, res);
} },
{ "/rest/v1/global_params/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvGlobalParamsView(), req, res);
} },
{ "/rest/v1/global_params/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlGlobalParamsView(), req, res);
} },
{ "/rest/v1/global_vars/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvGlobalVarsView(), req, res);
} },
{ "/rest/v1/global_vars/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlGlobalVarsView(), req, res);
} },
{ "/rest/v1/alert_params/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvAlertParamsView(), req, res);
} },
{ "/rest/v1/alert_params/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlAlertParamsView(), req, res);
} },
{ "/rest/v1/alerts/stateful_list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvStatefulAlertsView(), req, res);
} },
{ "/rest/v1/alerts/stateful_list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlStatefulAlertsView(), req, res);
} },
{ "/rest/v1/alerts/last_emitted.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvLastEmittedAlertsView(), req, res);
} },
{ "/rest/v1/alerts/last_emitted.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlLastEmittedAlertsView(), req, res);
} },
{ "/rest/v1/alerts_subscriptions/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeItemsAsCsv(webconsole->scheduler()->config().alerterConfig()
                             .alertSubscriptions(), req, res);
} },
{ "/rest/v1/alerts_subscriptions/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlAlertSubscriptionsView(), req, res);
} },
{ "/rest/v1/alerts_settings/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeItemsAsCsv(webconsole->scheduler()->config().alerterConfig()
                             .alertSettings(), req, res);
} },
{ "/rest/v1/alerts_settings/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlAlertSettingsView(), req, res);
} },
{ "/rest/v1/gridboards/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return sortAndWriteItemsAsCsv(
            webconsole->scheduler()->config().alerterConfig().gridboards(),
            req, res);
} },
{ "/rest/v1/gridboards/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlGridboardsView(), req, res);
} },
{ "/rest/v1/gridboards/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      QString gridboardid = req.url().path().mid(matchedLength)
          .replace(htmlSuffixRe, QString());
      Gridboard gridboard = webconsole->scheduler()->alerter()
          ->gridboard(gridboardid);
      res.setContentType("text/html;charset=UTF-8");
      res.output()->write(gridboard.toHtml().toUtf8().constData());
      return true;
}, true },
{ "/rest/v1/configs/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvConfigsView(), req, res);
} },
{ "/rest/v1/configs/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlConfigsView(), req, res);
} },
{ "/rest/v1/configs/history.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvConfigHistoryView(), req, res);
} },
{ "/rest/v1/configs/history.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlConfigHistoryView(), req, res);
} },
{ "/rest/v1/configs/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD|HttpRequest::POST
                          |HttpRequest::PUT, req, res))
        return true;
      if (req.method() == HttpRequest::POST
          || req.method() == HttpRequest::PUT)
        webconsole->configUploadHandler()
            ->handleRequest(req, res, processingContext);
      else {
        if (webconsole->configRepository()) {
          QString configid = req.url().path().mid(matchedLength)
              .replace(pfSuffixRe, QString());
          SchedulerConfig config
              = (configid == "current")
              ? webconsole->configRepository()->activeConfig()
              : webconsole->configRepository()->config(configid);
          if (config.isNull()) {
            res.setStatus(404);
            res.output()->write("no config found with this id\n");
          } else {
            res.setContentType("text/plain;charset=UTF-8");
            config.originalPfNode()
                .writePf(res.output(), PfOptions().setShouldIndent()
                         .setShouldWriteContentBeforeSubnodes()
                         .setShouldIgnoreComment(false));
          }
        } else {
          res.setStatus(500);
          res.output()->write("no config repository is set\n");
        }
      }
      return true;
}, true },
{ "/rest/v1/logs/logfiles.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeItemsAsCsv(
            webconsole->scheduler()->config().logfiles(), req, res);
} },
{ "/rest/v1/logs/logfiles.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlLogFilesView(), req, res);
} },
{ "/rest/v1/calendars/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return sortAndWriteItemsAsCsv(
            webconsole->scheduler()->config().namedCalendars().values(),
            req, res);
} },
{ "/rest/v1/calendars/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlCalendarsView(), req, res);
} },
{ "/rest/v1/taskinstances/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvTaskInstancesView(), req, res);
} },
{ "/rest/v1/taskinstances/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlTaskInstancesView(), req, res);
} },
{ "/rest/v1/taskinstances/current/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return sortAndWriteItemsAsCsv(
            webconsole->scheduler()->unfinishedTaskInstances().values(),
            req, res);
    } },
{ "/rest/v1/taskinstances/current/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return sortAndWriteItemsAsHtmlTable(
            webconsole->scheduler()->unfinishedTaskInstances().values(),
            req, res);
} },
{ "/rest/v1/scheduler_events/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvSchedulerEventsView(), req, res);
} },
{ "/rest/v1/scheduler_events/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlSchedulerEventsView(), req, res);
} },
{ "/rest/v1/scheduler/stats.json", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      QJsonObject stats;
      for (auto key: _serverStats.keys()) {
        auto value = _serverStats.value(key)(webconsole, key);
        JsonFormats::recursive_insert(
              stats, key, QJsonValue::fromVariant(value));
      }
      QByteArray data = QJsonDocument(stats).toJson();
      res.setContentType("application/json;charset=UTF-8");
      res.setContentLength(data.size());
      if (req.method() != HttpRequest::HEAD)
        res.output()->write(data);
      return true;
} },
{ "/rest/v1/notices/lastposted.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvLastPostedNoticesView(), req, res);
} },
{ "/rest/v1/notices/lastposted.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlLastPostedNoticesView20(), req, res);
} },
{ "/rest/v1/logs/last_audit_entries.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeItemsAsCsv(webconsole->auditLogItems(), req, res);

} },
{ "/rest/v1/logs/last_info_entries.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeItemsAsCsv(webconsole->infoLogItems(), req, res);

} },
{ "/rest/v1/logs/last_info_entries.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlInfoLogView(), req, res);
} },
{ "/rest/v1/logs/last_warning_entries.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeItemsAsCsv(webconsole->warningLogItems(), req, res);
} },
{ "/rest/v1/logs/last_warning_entries.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlWarningLogView(), req, res);
} },
{ "/rest/v1/logs/entries.txt", [](
    WebConsole *, HttpRequest req, HttpResponse res, ParamsProviderMerger *,
    int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      QStringList paths;
      if (req.param(QStringLiteral("files")) == "current")
        paths = QStringList(Log::pathToLastFullestLog());
      else
        paths = Log::pathsToFullestLogs();
      if (paths.isEmpty()) {
        res.setStatus(404);
        res.output()->write("No log file found.");
      } else {
        res.setContentType("text/plain;charset=UTF-8");
        QString filter = req.param("filter"), regexp = req.param("regexp");
        copyFilteredFiles(paths, res.output(),
                          regexp.isEmpty() ? filter : regexp,
                          !regexp.isEmpty());
      }
      return true;
} },
{ "/rest/v1/tasks/deployment_diagram.svg", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return webconsole->tasksDeploymentDiagram()
          ->handleRequest(req, res, processingContext);
} },
{ "/rest/v1/tasks/deployment_diagram.dot", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writePlainText(webconsole->tasksDeploymentDiagram()
                            ->source(0).toUtf8(), req, res,
                            GRAPHVIZ_MIME_TYPE);
} },
{ "/rest/v1/tasks/trigger_diagram.svg", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return webconsole->tasksTriggerDiagram()
          ->handleRequest(req, res, processingContext);
} },
{ "/rest/v1/tasks/trigger_diagram.dot", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writePlainText(webconsole->tasksTriggerDiagram()
                            ->source(0).toUtf8(), req, res,
                            GRAPHVIZ_MIME_TYPE);
} },
{ "/rest/v1/resources/tasks_resources_hosts_diagram.svg", [](
                                           WebConsole *webconsole, HttpRequest req, HttpResponse res,
                                           ParamsProviderMerger *processingContext, int) {
   if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
     return true;
   return webconsole->tasksResourcesHostsDiagram()
       ->handleRequest(req, res, processingContext);
 } },
{ "/rest/v1/resources/tasks_resources_hosts_diagram.dot", [](
                                           WebConsole *webconsole, HttpRequest req, HttpResponse res,
                                           ParamsProviderMerger *, int) {
   if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
     return true;
   return writePlainText(webconsole->tasksResourcesHostsDiagram()
                             ->source(0).toUtf8(), req, res,
                         GRAPHVIZ_MIME_TYPE);
 } },
};

RadixTree<QString> _staticRedirects {
  { { "/", "/console" }, "console/overview.html" },
  { { "/console/", "/console/index.html"}, "overview.html" },
};

bool WebConsole::handleRequest(
    HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext) {
  if (redirectForUrlCleanup(req, res, processingContext))
    return true;
  QString path = req.url().path();
  QString userid = processingContext->paramValue("userid").toString();
  ParamsProviderMergerRestorer restorer(processingContext);
  ServerStatsProvider webconsoleParams(this);
  processingContext->append(&webconsoleParams);
  processingContext->append(_scheduler->globalParams());
  // compute !pathtoroot now, to allow overriding url path with html files paths
  // unrelated to the url and keep !pathtoroot ok
  _wuiHandler->computePathToRoot(req, processingContext);
  if (!_scheduler) {
    res.setStatus(500);
    res.output()->write("Scheduler is not available.");
    return true;
  }
  auto staticRedirect = _staticRedirects.value(path);
  if (!staticRedirect.isEmpty()) {
    res.redirect(staticRedirect, HttpResponse::HTTP_Moved_Permanently);
    return true;
  }
  if (_authorizer && !_authorizer->authorize(userid, req.methodName(), path)) {
    res.setStatus(HttpResponse::HTTP_Forbidden);
    res.clearCookie("message", "/");
    // LATER nicer display
    res.output()->write("Permission denied.");
    return true;
  }
  int matchedLength;
  auto handler = _handlers.value(path, &matchedLength);
  //_handlers.dumpContent();
  //qDebug() << "handling" << path << !!handler << matchedLength;
  if (handler) {
    return handler(this, req, res, processingContext, matchedLength);
  }
  res.setStatus(404);
  res.output()->write("Not found.");
  return true;
}

void WebConsole::setScheduler(Scheduler *scheduler) {
  _scheduler = scheduler;
  if (_scheduler) {
    _tasksModel->setDocumentManager(scheduler);
    _hostsModel->setDocumentManager(scheduler);
    _clustersModel->setDocumentManager(scheduler);
    connect(_scheduler, &Scheduler::itemChanged,
            _freeResourcesModel, &HostsResourcesAvailabilityModel::changeItem);
    connect(_scheduler, &Scheduler::hostsResourcesAvailabilityChanged,
            _freeResourcesModel, &HostsResourcesAvailabilityModel::hostsResourcesAvailabilityChanged);
    connect(_scheduler, &Scheduler::itemChanged,
            _resourcesLwmModel, &HostsResourcesAvailabilityModel::changeItem);
    connect(_scheduler, &Scheduler::hostsResourcesAvailabilityChanged,
            _resourcesLwmModel, &HostsResourcesAvailabilityModel::hostsResourcesAvailabilityChanged);
    _globalParamsModel->connectToDocumentManager<QronConfigDocumentManager>(
          _scheduler, _scheduler->globalParams(), "globalparams", "globalparam",
          &QronConfigDocumentManager::paramsChanged,
          &QronConfigDocumentManager::changeParams);
    _globalVarsModel->connectToDocumentManager<QronConfigDocumentManager>(
          _scheduler, _scheduler->globalVars(), "globalvars",
          "globalvars", &QronConfigDocumentManager::paramsChanged,
          &QronConfigDocumentManager::changeParams);
    connect(_scheduler, &Scheduler::paramsChanged,
            this, &WebConsole::paramsChanged);
    connect(_scheduler->alerter(), &Alerter::paramsChanged,
            _alertParamsModel, &ParamSetModel::changeParams);
    connect(_scheduler->alerter(), &Alerter::statefulAlertChanged,
            _statefulAlertsModel, &SharedUiItemsTableModel::changeItem);
    connect(_scheduler->alerter(), &Alerter::alertNotified,
            _lastEmittedAlertsModel, &SharedUiItemsLogModel::logItem);
    connect(_scheduler->alerter(), &Alerter::configChanged,
            this, &WebConsole::alerterConfigChanged);
    _taskInstancesHistoryModel->setDocumentManager(scheduler);
    _unfinishedTaskInstancesModel->setDocumentManager(scheduler);
    _calendarsModel->setDocumentManager(scheduler);
    _taskGroupsModel->setDocumentManager(scheduler);
    connect(_scheduler, &Scheduler::globalEventSubscriptionsChanged,
            _schedulerEventsModel, &SchedulerEventsModel::globalEventSubscriptionsChanged);
    connect(_scheduler, &Scheduler::noticePosted,
            _lastPostedNoticesModel, &LastOccuredTextEventsModel::eventOccured);
    connect(_scheduler, &Scheduler::accessControlConfigurationChanged,
            this, &WebConsole::enableAccessControl);
    connect(_scheduler, &Scheduler::logConfigurationChanged,
            _logConfigurationModel, &SharedUiItemsTableModel::setItems);
  }
}

void WebConsole::setConfigPaths(QString configFilePath,
                                QString configRepoPath) {
  _configFilePath = configFilePath.isEmpty() ? "(none)" : configFilePath;
  _configRepoPath = configRepoPath.isEmpty() ? "(none)" : configRepoPath;
}

void WebConsole::setConfigRepository(ConfigRepository *configRepository) {
  _configRepository = configRepository;
  _configUploadHandler->setConfigRepository(_configRepository);
  if (_configRepository) {
    // Configs
    connect(_configRepository, &ConfigRepository::configAdded,
            _htmlConfigsDelegate, &HtmlSchedulerConfigItemDelegate::configAdded);
    connect(_configRepository, &ConfigRepository::configRemoved,
            _htmlConfigsDelegate, &HtmlSchedulerConfigItemDelegate::configRemoved);
    connect(_configRepository, &ConfigRepository::configActivated,
            _htmlConfigsDelegate, &HtmlSchedulerConfigItemDelegate::configActivated);
    connect(_configRepository, &ConfigRepository::configAdded,
            _configsModel, &ConfigsModel::configAdded);
    connect(_configRepository, &ConfigRepository::configRemoved,
            _configsModel, &ConfigsModel::configRemoved);
    connect(_configRepository, &ConfigRepository::configActivated,
            _configsModel, &ConfigsModel::configActivated);
    connect(_configRepository, &ConfigRepository::configActivated,
            _htmlConfigsView, &HtmlTableView::invalidateCache); // needed for isActive and actions columns
    // Config History
    connect(_configRepository, &ConfigRepository::historyReset,
            _configHistoryModel, &ConfigHistoryModel::historyReset);
    connect(_configRepository, &ConfigRepository::historyEntryAppended,
            _configHistoryModel, &ConfigHistoryModel::historyEntryAppended);
    connect(_configRepository, &ConfigRepository::configActivated,
            _htmlConfigHistoryDelegate, &HtmlSchedulerConfigItemDelegate::configActivated);
    connect(_configRepository, &ConfigRepository::configAdded,
            _htmlConfigHistoryDelegate, &HtmlSchedulerConfigItemDelegate::configAdded);
    connect(_configRepository, &ConfigRepository::configRemoved,
            _htmlConfigHistoryDelegate, &HtmlSchedulerConfigItemDelegate::configRemoved);
    connect(_configRepository, &ConfigRepository::configRemoved,
            _htmlConfigHistoryView, &HtmlTableView::invalidateCache); // needed for config id link removal
    connect(_configRepository, &ConfigRepository::configActivated,
            _htmlConfigHistoryView, &HtmlTableView::invalidateCache); // needed for actions column
    // Other models and views
    connect(_configRepository, &ConfigRepository::configActivated,
            this, &WebConsole::computeDiagrams);
    connect(_configRepository, &ConfigRepository::configActivated,
            _resourcesConsumptionModel, &ResourcesConsumptionModel::configActivated);
  }
}

void WebConsole::setAuthorizer(InMemoryRulesAuthorizer *authorizer) {
  _authorizer = authorizer;
  enableAccessControl(false);
}

void WebConsole::enableAccessControl(bool enabled) {
  if (!_authorizer)
    return;
  // LATER this is not transactional and thus may issue 403's during conf reload
  if (enabled) {
    _authorizer->clearRules()
        // anyone for static resources
        .allow("", "", "^/console/(css|jsp|js|img|font)/")
        // anyone for test page and user manual
        .allow("", "", "^/console/(test|user-manual)\\.html$")
        // read for read-only rest calls
        .allow("read", "^GET|HEAD$", "^/rest/")
        // operate for operation including other rest calls
        .allow("operate", "", "^/console/confirm/")
        .allow("operate", "", "^/console/tasks/request/")
        .allow("operate", "", "^/do/")
        .allow("operate", "", "^/rest/")
        // nobody else on operation and rest paths
        .deny("", "", "^/console/confirm/")
        .deny("", "", "^/console/tasks/request/")
        .deny("", "", "^/do/")
        .deny("", "", "^/rest/")
        // read for everything else on the console
        .allow("read", "", "^/console")
        // deny everything else
        .deny();
  } else {
    _authorizer->clearRules()
        .allow(); // anyone for anything
  }
}

void WebConsole::paramsChanged(
    ParamSet newParams, ParamSet oldParams, QString setId) {
  Q_UNUSED(oldParams)
  if (setId != QStringLiteral("globalparams"))
    return;
  QString s = newParams.rawValue("webconsole.showaudituser.regexp");
  _showAuditUser = s.isNull()
      ? QRegularExpression() : QRegularExpression(s);
  s = newParams.rawValue("webconsole.hideaudituser.regexp");
  _hideAuditUser = s.isNull()
      ? QRegularExpression() : QRegularExpression(s);
  s = newParams.rawValue("webconsole.showauditevent.regexp");
  _showAuditEvent = s.isNull()
      ? QRegularExpression() : QRegularExpression(s);
  s = newParams.rawValue("webconsole.hideauditevent.regexp");
  _hideAuditEvent = s.isNull()
      ? QRegularExpression() : QRegularExpression(s);
  QString customactions_taskslist =
      newParams.rawValue("webconsole.customactions.taskslist");
  _tasksModel->setCustomActions(customactions_taskslist);
  QString customactions_instanceslist =
      newParams.rawValue("webconsole.customactions.instanceslist");
  _unfinishedTaskInstancesModel->setCustomActions(customactions_instanceslist);
  _taskInstancesHistoryModel->setCustomActions(customactions_instanceslist);
  int rowsPerPage = newParams.valueAsInt(
        "webconsole.htmltables.rowsperpage", 100);
  int cachedRows = newParams.valueAsInt(
        "webconsole.htmltables.cachedrows", 500);
  for (QObject *child: children()) {
    auto *htmlView = qobject_cast<HtmlTableView*>(child);
    auto *csvView = qobject_cast<CsvTableView*>(child);
    if (htmlView) {
      if (htmlView != _htmlWarningLogView10
          && htmlView != _htmlUnfinishedTaskInstancesView
          && htmlView != _htmlLastPostedNoticesView20
          && htmlView != _htmlRaisedAlertsView)
        htmlView->setRowsPerPage(rowsPerPage);
      htmlView->setCachedRows(cachedRows);
    }
    if (csvView) {
      csvView->setCachedRows(cachedRows);
    }
  }
}

void WebConsole::computeDiagrams(SchedulerConfig config) {
  QHash<QString,QString> diagrams
      = GraphvizDiagramsBuilder::configDiagrams(config);
  _tasksDeploymentDiagram->setSource(diagrams.value("tasksDeploymentDiagram"));
  _tasksTriggerDiagram->setSource(diagrams.value("tasksTriggerDiagram"));
  _tasksResourcesHostsDiagram->setSource(
      diagrams.value("tasksResourcesHostsDiagram"));
}

void WebConsole::alerterConfigChanged(AlerterConfig config) {
  _alerterConfig = config;
  _alertSubscriptionsModel->setItems(config.alertSubscriptions());
  _alertSettingsModel->setItems(config.alertSettings());
  _alertChannelsModel->clear();
  _gridboardsModel->setItems((config.gridboards()));
  for (const QString &channel: config.channelsNames())
    _alertChannelsModel->setCellValue(channel, "enabled", "true");
}
