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
#include "webconsole.h"
#include "util/ioutils.h"
#include <QFile>
#include <QThread>
#include "sched/taskinstance.h"
#include "sched/qrond.h"
#include <QCoreApplication>
#include "util/htmlutils.h"
#include <QUrlQuery>
#include "util/timeformats.h"
#include "textview/htmlitemdelegate.h"
#include "htmltaskitemdelegate.h"
#include "htmltaskinstanceitemdelegate.h"
#include "htmlalertitemdelegate.h"
#include "ui/graphvizdiagramsbuilder.h"
#include "htmllogentryitemdelegate.h"
#include "alert/alert.h"
#include "alert/gridboard.h"
#include "config/step.h"
#include "util/radixtree.h"
#include <functional>
#include "util/characterseparatedexpression.h"

#define SHORT_LOG_MAXROWS 100
#define SHORT_LOG_ROWSPERPAGE 10
#define UNFINISHED_TASK_INSTANCE_MAXROWS 1000
#define SVG_BELONG_TO_WORKFLOW "<svg height=\"30\" width=\"600\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"><a xlink:title=\"%1\"><text x=\"0\" y=\"15\">This task belongs to workflow \"%1\".</text></a></svg>"
#define SVG_NOT_A_WORKFLOW "<svg height=\"30\" width=\"600\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"><text x=\"0\" y=\"15\">This task is not a workflow.</text></svg>"
#define ISO8601 QStringLiteral("yyyy-MM-dd hh:mm:ss")
//#define GRAPHVIZ_MIME_TYPE "text/vnd.graphviz;charset=UTF-8"
#define GRAPHVIZ_MIME_TYPE "text/plain;charset=UTF-8"

static PfNode nodeWithValidPattern =
    PfNode("dummy", "dummy").setAttribute("pattern", ".*")
    .setAttribute("dimension", "dummy");

static QRegularExpression htmlSuffixRe("\\.html$");
static QRegularExpression pfSuffixRe("\\.pf$");

WebConsole::WebConsole() : _thread(new QThread), _scheduler(0),
  _configRepository(0), _authorizer(0),
  _readOnlyResourcesCache(new ReadOnlyResourcesCache(this)) {
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
  _configUploadHandler = new ConfigUploadHandler("", 1, this);

  // models
  _hostsModel = new SharedUiItemsTableModel(Host(PfNode("template")), this);
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
  _freeResourcesModel->setShouldSortRows();
  _freeResourcesModel->setShouldSortColumns();
  _resourcesLwmModel = new HostsResourcesAvailabilityModel(
        this, HostsResourcesAvailabilityModel::LwmOverConfigured);
  _resourcesLwmModel->setShouldSortRows();
  _resourcesLwmModel->setShouldSortColumns();
  _resourcesConsumptionModel = new ResourcesConsumptionModel(this);
  _globalParamsModel = new ParamSetModel(this);
  _globalSetenvModel = new ParamSetModel(this);
  _globalUnsetenvModel = new ParamSetModel(this);
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
  _sortedRaisedAlertModel->setFilterRegExp("raised|dropping");
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
  _taskInstancesHistoryModel = new TaskInstancesModel(this);
  _taskInstancesHistoryModel->setItemQualifierFilter("taskinstance");
  _unfinishedTaskInstancetModel =
      new TaskInstancesModel(this, UNFINISHED_TASK_INSTANCE_MAXROWS, false);
  _unfinishedTaskInstancetModel->setItemQualifierFilter("taskinstance");
  _tasksModel = new TasksModel(this);
  _tasksModel->setItemQualifierFilter("task");
  _mainTasksModel = new QSortFilterProxyModel(this);
  _mainTasksModel->setFilterKeyColumn(31);
  _mainTasksModel->setFilterRegExp("^$");
  _mainTasksModel->sort(11);
  _mainTasksModel->setSourceModel(_tasksModel);
  _subtasksModel = new QSortFilterProxyModel(this);
  _subtasksModel->setFilterKeyColumn(31);
  _subtasksModel->setFilterRegExp(".+");
  _subtasksModel->sort(11);
  _subtasksModel->setSourceModel(_tasksModel);
  _schedulerEventsModel = new SchedulerEventsModel(this);
  _taskGroupsModel = new TaskGroupsModel(this);
  _taskGroupsModel->setItemQualifierFilter("taskgroup");
  _sortedTaskGroupsModel = new QSortFilterProxyModel(this);
  _sortedTaskGroupsModel->sort(0);
  _sortedTaskGroupsModel->setSourceModel(_taskGroupsModel);
  _alertChannelsModel = new TextMatrixModel(this);
  _logConfigurationModel = new LogFilesModel(this);
  _calendarsModel = new SharedUiItemsTableModel(this);
  _calendarsModel->setHeaderDataFromTemplate(Calendar(PfNode("calendar")));
  _calendarsModel->setItemQualifierFilter("calendar");
  _sortedCalendarsModel = new QSortFilterProxyModel(this);
  _sortedCalendarsModel->sort(1);
  _sortedCalendarsModel->setSourceModel(_calendarsModel);
  _stepsModel= new SharedUiItemsTableModel(this);
  _stepsModel->setHeaderDataFromTemplate(
        Step(PfNode("start"), 0, TaskGroup(), QString(),
             QHash<QString,Calendar>()));
  _stepsModel->setItemQualifierFilter("step");
  _sortedStepsModel = new QSortFilterProxyModel(this);
  _sortedStepsModel->setSourceModel(_stepsModel);
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
  ((HtmlItemDelegate*)_htmlHostsListView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"icon-hdd\"></i>&nbsp;")
      ->setPrefixForColumnHeader(2, "<i class=\"icon-fast-food\"></i>&nbsp;");
  _wuiHandler->addView(_htmlHostsListView);
  _htmlClustersListView = new HtmlTableView(this, "clusterslist");
  _htmlClustersListView->setModel(_sortedClustersModel);
  _htmlClustersListView->setEmptyPlaceholder("(no cluster)");
  ((HtmlItemDelegate*)_htmlClustersListView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"icon-shuffle\"></i>&nbsp;")
      ->setPrefixForColumnHeader(1, "<i class=\"icon-hdd\"></i>&nbsp;");
  _wuiHandler->addView(_htmlClustersListView);
  _htmlFreeResourcesView = new HtmlTableView(this, "freeresources");
  _htmlFreeResourcesView->setModel(_freeResourcesModel);
  _htmlFreeResourcesView->setRowHeaders();
  _htmlFreeResourcesView->setEmptyPlaceholder("(no resource definition)");
  ((HtmlItemDelegate*)_htmlFreeResourcesView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"icon-fast-food\"></i>&nbsp;")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"icon-hdd\"></i>&nbsp;");
  _wuiHandler->addView(_htmlFreeResourcesView);
  _htmlResourcesLwmView = new HtmlTableView(this, "resourceslwm");
  _htmlResourcesLwmView->setModel(_resourcesLwmModel);
  _htmlResourcesLwmView->setRowHeaders();
  _htmlResourcesLwmView->setEmptyPlaceholder("(no resource definition)");
  ((HtmlItemDelegate*)_htmlResourcesLwmView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"icon-fast-food\"></i>&nbsp;")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"icon-hdd\"></i>&nbsp;");
  _wuiHandler->addView(_htmlResourcesLwmView);
  _htmlResourcesConsumptionView =
      new HtmlTableView(this, "resourcesconsumption");
  _htmlResourcesConsumptionView->setModel(_resourcesConsumptionModel);
  _htmlResourcesConsumptionView->setRowHeaders();
  _htmlResourcesConsumptionView
      ->setEmptyPlaceholder("(no resource definition)");
  ((HtmlItemDelegate*)_htmlResourcesConsumptionView->itemDelegate())
      ->setPrefixForColumnHeader(HtmlItemDelegate::AllSections,
                                 "<i class=\"icon-hdd\"></i>&nbsp;")
      ->setPrefixForRowHeader(HtmlItemDelegate::AllSections,
                              "<i class=\"icon-cog\"></i>&nbsp;")
      ->setPrefixForRowHeader(0, "")
      ->setPrefixForRow(0, "<strong>")
      ->setSuffixForRow(0, "</strong>");
  _wuiHandler->addView(_htmlResourcesConsumptionView);
  _htmlGlobalParamsView = new HtmlTableView(this, "globalparams");
  _htmlGlobalParamsView->setModel(_globalParamsModel);
  cols.clear();
  cols << 0 << 1;
  _htmlGlobalParamsView->setColumnIndexes(cols);
  _wuiHandler->addView(_htmlGlobalParamsView);
  _htmlGlobalSetenvView = new HtmlTableView(this, "globalsetenv");
  _htmlGlobalSetenvView->setModel(_globalSetenvModel);
  cols.clear();
  cols << 0 << 1;
  _htmlGlobalSetenvView->setColumnIndexes(cols);
  _wuiHandler->addView(_htmlGlobalSetenvView);
  _htmlGlobalUnsetenvView = new HtmlTableView(this, "globalunsetenv");
  _htmlGlobalUnsetenvView->setModel(_globalUnsetenvModel);
  _wuiHandler->addView(_htmlGlobalUnsetenvView);
  cols.clear();
  cols << 0;
  _htmlGlobalUnsetenvView->setColumnIndexes(cols);
  _htmlAlertParamsView = new HtmlTableView(this, "alertparams");
  _htmlAlertParamsView->setModel(_alertParamsModel);
  _wuiHandler->addView(_htmlAlertParamsView);
  _htmlStatefulAlertsView = new HtmlTableView(this, "statefulalerts");
  _htmlStatefulAlertsView->setModel(_sortedStatefulAlertsModel);
  _htmlStatefulAlertsView->setEmptyPlaceholder("(no alert)");
  cols.clear();
  cols << 0 << 2 << 3 << 4 << 5;
  _htmlStatefulAlertsView->setColumnIndexes(cols);
  QHash<QString,QString> alertsIcons;
  alertsIcons.insert(Alert::statusToString(Alert::Nonexistent),
                     "<i class=\"icon-bell\"></i>&nbsp;");
  alertsIcons.insert(Alert::statusToString(Alert::Rising),
                     "<i class=\"icon-bell-empty\"></i>&nbsp;<strike>");
  alertsIcons.insert(Alert::statusToString(Alert::MayRise),
                     "<i class=\"icon-bell-empty\"></i>&nbsp;<strike>");
  alertsIcons.insert(Alert::statusToString(Alert::Raised),
                     "<i class=\"icon-bell\"></i>&nbsp;");
  alertsIcons.insert(Alert::statusToString(Alert::Dropping),
                     "<i class=\"icon-bell\"></i>&nbsp;");
  alertsIcons.insert(Alert::statusToString(Alert::Canceled),
                     "<i class=\"icon-check\"></i>&nbsp;");
  _htmlStatefulAlertsView->setItemDelegate(
        new HtmlAlertItemDelegate(_htmlStatefulAlertsView, true));
  qobject_cast<HtmlItemDelegate*>(_htmlStatefulAlertsView->itemDelegate())
      ->setPrefixForColumn(0, "%1", 1, alertsIcons);
  _wuiHandler->addView(_htmlStatefulAlertsView);
  _htmlRaisedAlertsView = new HtmlTableView(this, "raisedalerts");
  _htmlRaisedAlertsView->setRowsPerPage(10);
  _htmlRaisedAlertsView->setModel(_sortedRaisedAlertModel);
  _htmlRaisedAlertsView->setEmptyPlaceholder("(no alert)");
  cols.clear();
  cols << 0 << 2 << 4 << 5;
  _htmlRaisedAlertsView->setColumnIndexes(cols);
  _htmlRaisedAlertsView->setItemDelegate(
        new HtmlAlertItemDelegate(_htmlRaisedAlertsView, true));
  qobject_cast<HtmlItemDelegate*>(_htmlRaisedAlertsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"icon-bell\"></i>&nbsp;");
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
  cols.clear();
  cols << _lastEmittedAlertsModel->timestampColumn() << 7 << 5;
  _htmlLastEmittedAlertsView->setColumnIndexes(cols);
  _wuiHandler->addView(_htmlLastEmittedAlertsView);
  _htmlAlertSubscriptionsView = new HtmlTableView(this, "alertsubscriptions");
  _htmlAlertSubscriptionsView->setModel(_alertSubscriptionsModel);
  QHash<QString,QString> alertSubscriptionsIcons;
  alertSubscriptionsIcons.insert("stop", "<i class=\"icon-stop\"></i>&nbsp;");
  ((HtmlItemDelegate*)_htmlAlertSubscriptionsView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"icon-filter\"></i>&nbsp;")
      ->setPrefixForColumn(2, "%1", 2, alertSubscriptionsIcons);
  cols.clear();
  cols << 1 << 2 << 3 << 4 << 5 << 12;
  _htmlAlertSubscriptionsView->setColumnIndexes(cols);
  _wuiHandler->addView(_htmlAlertSubscriptionsView);
  _htmlAlertSettingsView = new HtmlTableView(this, "alertsettings");
  _htmlAlertSettingsView->setModel(_alertSettingsModel);
  ((HtmlItemDelegate*)_htmlAlertSettingsView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"icon-filter\"></i>&nbsp;");
  cols.clear();
  cols << 1 << 2;
  _htmlAlertSettingsView->setColumnIndexes(cols);
  _wuiHandler->addView(_htmlAlertSettingsView);
  _htmlGridboardsView = new HtmlTableView(this, "gridboards");
  _htmlGridboardsView->setModel(_sortedGridboardsModel);
  ((HtmlItemDelegate*)_htmlGridboardsView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"icon-gauge\"></i>&nbsp;"
                              "<a href=\"gridboards/%1\">", 0)
      ->setSuffixForColumn(1, "</a>");
  cols.clear();
  cols << 1 << 2 << 3;
  _htmlGridboardsView->setColumnIndexes(cols);
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
  _htmlTaskInstancesView20 =
      new HtmlTableView(this, "taskinstances20",
                        _unfinishedTaskInstancetModel->maxrows(), 20);
  _htmlTaskInstancesView20->setModel(_unfinishedTaskInstancetModel);
  QHash<QString,QString> taskInstancesTrClasses;
  taskInstancesTrClasses.insert("failure", "danger");
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
  _htmlTaskInstancesView = new HtmlTableView(
        this, "taskinstances", _taskInstancesHistoryModel->maxrows());
  _htmlTaskInstancesView->setModel(_taskInstancesHistoryModel);
  _htmlTaskInstancesView->setTrClass("%1", 2, taskInstancesTrClasses);
  _htmlTaskInstancesView->setEmptyPlaceholder("(no recent task)");
  cols.clear();
  cols << 0 << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8;
  _htmlTaskInstancesView->setColumnIndexes(cols);
  _htmlTaskInstancesView
      ->setItemDelegate(new HtmlTaskInstanceItemDelegate(_htmlTaskInstancesView));
  _wuiHandler->addView(_htmlTaskInstancesView);
  _htmlTasksScheduleView = new HtmlTableView(this, "tasksschedule");
  _htmlTasksScheduleView->setModel(_mainTasksModel);
  _htmlTasksScheduleView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 11 << 2 << 5 << 6 << 19 << 10 << 17 << 18;
  _htmlTasksScheduleView->setColumnIndexes(cols);
  _htmlTasksScheduleView->enableRowAnchor(11);
  _htmlTasksScheduleView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksScheduleView));
  _wuiHandler->addView(_htmlTasksScheduleView);
  _htmlTasksConfigView = new HtmlTableView(this, "tasksconfig");
  _htmlTasksConfigView->setModel(_tasksModel);
  _htmlTasksConfigView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 1 << 0 << 3 << 5 << 4 << 28 << 8 << 12 << 18;
  _htmlTasksConfigView->setColumnIndexes(cols);
  _htmlTasksConfigView->enableRowAnchor(11);
  _htmlTasksConfigView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksConfigView));
  _wuiHandler->addView(_htmlTasksConfigView);
  _htmlTasksParamsView = new HtmlTableView(this, "tasksparams");
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
      new HtmlTableView(this, "taskslist");
  _htmlTasksListView->setModel(_tasksModel);
  _htmlTasksListView->setEmptyPlaceholder("(no task in configuration)");
  _htmlTasksListView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksListView));
  _htmlTasksEventsView = new HtmlTableView(this, "tasksevents");
  _htmlTasksEventsView->setModel(_mainTasksModel);
  _htmlTasksEventsView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 11 << 6 << 14 << 15 << 16 << 18;
  _htmlTasksEventsView->setColumnIndexes(cols);
  _htmlTasksEventsView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksEventsView));
  _wuiHandler->addView(_htmlTasksEventsView);
  _htmlSchedulerEventsView = new HtmlTableView(this, "schedulerevents");
  ((HtmlItemDelegate*)_htmlSchedulerEventsView->itemDelegate())
      ->setPrefixForColumnHeader(0, "<i class=\"icon-play\"></i>&nbsp;")
      ->setPrefixForColumnHeader(1, "<i class=\"icon-refresh\"></i>&nbsp;")
      ->setPrefixForColumnHeader(2, "<i class=\"icon-comment\"></i>&nbsp;")
      ->setPrefixForColumnHeader(3, "<i class=\"icon-file-text\"></i>&nbsp;")
      ->setPrefixForColumnHeader(4, "<i class=\"icon-play\"></i>&nbsp;")
      ->setPrefixForColumnHeader(5, "<i class=\"icon-check\"></i>&nbsp;")
      ->setPrefixForColumnHeader(6, "<i class=\"icon-minus-circled\"></i>&nbsp;")
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
      ->setPrefixForColumn(1, "<i class=\"icon-comment\"></i>&nbsp;");
  _wuiHandler->addView(_htmlLastPostedNoticesView20);
  _htmlTaskGroupsView = new HtmlTableView(this, "taskgroups");
  _htmlTaskGroupsView->setModel(_sortedTaskGroupsModel);
  _htmlTaskGroupsView->setEmptyPlaceholder("(no task group)");
  cols.clear();
  cols << 0 << 2 << 7 << 20 << 21;
  _htmlTaskGroupsView->setColumnIndexes(cols);
  ((HtmlItemDelegate*)_htmlTaskGroupsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"icon-cogs\"></i>&nbsp;");
  _wuiHandler->addView(_htmlTaskGroupsView);
  _htmlTaskGroupsEventsView = new HtmlTableView(this, "taskgroupsevents");
  _htmlTaskGroupsEventsView->setModel(_sortedTaskGroupsModel);
  _htmlTaskGroupsEventsView->setEmptyPlaceholder("(no task group)");
  cols.clear();
  cols << 0 << 14 << 15 << 16;
  _htmlTaskGroupsEventsView->setColumnIndexes(cols);
  ((HtmlItemDelegate*)_htmlTaskGroupsEventsView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"icon-cogs\"></i>&nbsp;")
      ->setPrefixForColumnHeader(14, "<i class=\"icon-play\"></i>&nbsp;")
      ->setPrefixForColumnHeader(15, "<i class=\"icon-check\"></i>&nbsp;")
      ->setPrefixForColumnHeader(16, "<i class=\"icon-minus-circled\"></i>&nbsp;");
  _wuiHandler->addView(_htmlTaskGroupsEventsView);
  _htmlAlertChannelsView = new HtmlTableView(this, "alertchannels");
  _htmlAlertChannelsView->setModel(_alertChannelsModel);
  _htmlAlertChannelsView->setRowHeaders();
  _wuiHandler->addView(_htmlAlertChannelsView);
  _htmlTasksResourcesView = new HtmlTableView(this, "tasksresources");
  _htmlTasksResourcesView->setModel(_tasksModel);
  _htmlTasksResourcesView->setEmptyPlaceholder("(no task)");
  cols.clear();
  cols << 11 << 17 << 8;
  _htmlTasksResourcesView->setColumnIndexes(cols);
  _htmlTasksResourcesView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksResourcesView));
  _wuiHandler->addView(_htmlTasksResourcesView);
  _htmlTasksAlertsView = new HtmlTableView(this, "tasksalerts");
  _htmlTasksAlertsView->setModel(_tasksModel);
  _htmlTasksAlertsView->setEmptyPlaceholder("(no task)");
  cols.clear();
  cols << 11 << 6 << 23 << 26 << 24 << 27 << 12 << 16 << 18;
  _htmlTasksAlertsView->setColumnIndexes(cols);
  _htmlTasksAlertsView->setItemDelegate(
        new HtmlTaskItemDelegate(_htmlTasksAlertsView));
  _wuiHandler->addView(_htmlTasksAlertsView);
  _htmlLogFilesView = new HtmlTableView(this, "logfiles");
  _htmlLogFilesView->setModel(_logConfigurationModel);
  _htmlLogFilesView->setEmptyPlaceholder("(no log file)");
  QHash<QString,QString> bufferLogFileIcons;
  bufferLogFileIcons.insert("true", "<i class=\"icon-download\"></i>&nbsp;");
  ((HtmlItemDelegate*)_htmlLogFilesView->itemDelegate())
      ->setPrefixForColumn(0, "<i class=\"icon-file-text\"></i>&nbsp;")
      ->setPrefixForColumn(2, "%1", 2, bufferLogFileIcons);
  _wuiHandler->addView(_htmlLogFilesView);
  _htmlCalendarsView = new HtmlTableView(this, "calendars");
  _htmlCalendarsView->setModel(_sortedCalendarsModel);
  cols.clear();
  cols << 1 << 2;
  _htmlCalendarsView->setColumnIndexes(cols);
  _htmlCalendarsView->setEmptyPlaceholder("(no named calendar)");
  ((HtmlItemDelegate*)_htmlCalendarsView->itemDelegate())
      ->setPrefixForColumn(1, "<i class=\"icon-calendar\"></i>&nbsp;");
  _wuiHandler->addView(_htmlCalendarsView);
  _htmlStepsView = new HtmlTableView(this, "steps");
  _htmlStepsView->setModel(_sortedStepsModel);
  _htmlStepsView->setEmptyPlaceholder("(no workflow step)");
  cols.clear();
  cols << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9;
  _htmlStepsView->setColumnIndexes(cols);
  _htmlStepsView->enableRowAnchor(0);
  _htmlStepsView->setItemDelegate(new HtmlStepItemDelegate(_htmlStepsView));
  _wuiHandler->addView(_htmlStepsView);
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
  cols.clear();
  cols << 1 << 2 << 3 << 4;
  _htmlConfigHistoryView->setColumnIndexes(cols);
  _htmlConfigHistoryDelegate =
      new HtmlSchedulerConfigItemDelegate(3, -1, 4, _htmlConfigsView);
  _htmlConfigHistoryView->setItemDelegate(_htmlConfigHistoryDelegate);
  _wuiHandler->addView(_htmlConfigHistoryView);

  // CSV views
  CsvTableView::setDefaultFieldQuote('"');
  CsvTableView::setDefaultReplacementChar(' ');
  _csvHostsListView = new CsvTableView(this);
  _csvHostsListView->setModel(_sortedHostsModel);
  _csvClustersListView = new CsvTableView(this);
  _csvClustersListView->setModel(_sortedClustersModel);
  _csvFreeResourcesView = new CsvTableView(this);
  _csvFreeResourcesView->setModel(_freeResourcesModel);
  _csvFreeResourcesView->setRowHeaders();
  _csvResourcesLwmView = new CsvTableView(this);
  _csvResourcesLwmView->setModel(_resourcesLwmModel);
  _csvResourcesLwmView->setRowHeaders();
  _csvResourcesConsumptionView = new CsvTableView(this);
  _csvResourcesConsumptionView->setModel(_resourcesConsumptionModel);
  _csvResourcesConsumptionView->setRowHeaders();
  _csvGlobalParamsView = new CsvTableView(this);
  _csvGlobalParamsView->setModel(_globalParamsModel);
  _csvGlobalSetenvView = new CsvTableView(this);
  _csvGlobalSetenvView->setModel(_globalSetenvModel);
  _csvGlobalUnsetenvView = new CsvTableView(this);
  _csvGlobalUnsetenvView->setModel(_globalUnsetenvModel);
  _csvAlertParamsView = new CsvTableView(this);
  _csvAlertParamsView->setModel(_alertParamsModel);
  _csvStatefulAlertsView = new CsvTableView(this);
  _csvStatefulAlertsView->setModel(_sortedStatefulAlertsModel);
  _csvLastEmittedAlertsView =
      new CsvTableView(this, _lastEmittedAlertsModel->maxrows());
  _csvLastEmittedAlertsView->setModel(_lastEmittedAlertsModel);
  _csvAlertSubscriptionsView = new CsvTableView(this);
  _csvAlertSubscriptionsView->setModel(_alertSubscriptionsModel);
  _csvAlertSettingsView = new CsvTableView(this);
  _csvAlertSettingsView->setModel(_alertSettingsModel);
  _csvGridboardsView = new CsvTableView(this);
  _csvGridboardsView->setModel(_sortedGridboardsModel);
  _csvLogView = new CsvTableView(this, _htmlInfoLogView->cachedRows());
  _csvLogView->setModel(_infoLogModel);
  _csvTaskInstancesView =
        new CsvTableView(this, _taskInstancesHistoryModel->maxrows());
  _csvTaskInstancesView->setModel(_taskInstancesHistoryModel);
  _csvTasksView = new CsvTableView(this);
  _csvTasksView->setModel(_tasksModel);
  _csvSchedulerEventsView = new CsvTableView(this);
  _csvSchedulerEventsView->setModel(_schedulerEventsModel);
  _csvLastPostedNoticesView =
      new CsvTableView(this, _lastPostedNoticesModel->maxrows());
  _csvLastPostedNoticesView->setModel(_lastPostedNoticesModel);
  _csvTaskGroupsView = new CsvTableView(this);
  _csvTaskGroupsView->setModel(_sortedTaskGroupsModel);
  _csvLogFilesView = new CsvTableView(this);
  _csvLogFilesView->setModel(_logConfigurationModel);
  _csvCalendarsView = new CsvTableView(this);
  _csvCalendarsView->setModel(_sortedCalendarsModel);
  _csvStepsView = new CsvTableView(this);
  _csvStepsView->setModel(_sortedStepsModel);
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

static QHash<QString,
std::function<QVariant(const WebConsole *, const QString &key)>> _consoleParams {
{ "scheduler.startdate", [](const WebConsole *console, const QString &) {
  // LATER use %=date syntax
  return console->scheduler()->startdate().toString(ISO8601);
} },
{ "scheduler.uptime", [](const WebConsole *console, const QString &) {
  return TimeFormats::toCoarseHumanReadableTimeInterval(
        console->scheduler()->startdate().msecsTo(QDateTime::currentDateTime()));
} },
{ "scheduler.configdate", [](const WebConsole *console, const QString &) {
  // LATER use %=date syntax
  return console->scheduler()->configdate().toString(ISO8601);
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

class WebConsoleParamsProvider : public ParamsProvider {
  WebConsole *_console;

public:
  WebConsoleParamsProvider(WebConsole *console) : _console(console) { }
  QVariant paramValue(const QString key, const QVariant defaultValue,
                      QSet<QString>) const {
    if (!_console || !_console->_scheduler) // should never happen
      return defaultValue;
    auto handler = _consoleParams.value(key);
    return handler ? handler(_console, key) : defaultValue;
  }
};

static bool enforceMethods(int methodsMask, HttpRequest req,
                             HttpResponse res) {
  if (req.method() & methodsMask)
    return true;
  res.setStatus(HttpResponse::HTTP_Method_Not_Allowed);
  res.output()->write("Method not allowed for this resource.");
  return false;
}

static bool writeHtmlView(HtmlTableView *view, HttpRequest req,
                          HttpResponse res) {
  QByteArray data = view->text().toUtf8();
  res.setContentType("text/html;charset=UTF-8");
  res.setContentLength(data.size());
  if (req.method() != HttpRequest::HEAD)
    res.output()->write(data);
  return true;
}

static bool writeCsvView(CsvTableView *view, HttpRequest req,
                         HttpResponse res) {
  QByteArray data = view->text().toUtf8();
  res.setContentType("text/csv;charset=UTF-8");
  res.setHeader("Content-Disposition", "attachment"); // TODO filename=table.csv");
  res.setContentLength(data.size());
  if (req.method() != HttpRequest::HEAD)
    res.output()->write(data);
  return true;
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
  // LATER switch to QRegularExpression (and use QRE::escape() for non-regexp)
  // LATER have a regexp cache
  foreach (const QString path, paths) {
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
      if (pattern.isEmpty())
        IOUtils::copy(output, &file);
      else
        IOUtils::grepWithContinuation(
              output, &file,
              QRegExp(pattern, Qt::CaseSensitive,
                      useRegexp ? QRegExp::RegExp2
                                : QRegExp::FixedString),
              "  ");
    } else {
      Log::warning() << "web console cannot open log file " << path
                     << " : error #" << file.error() << " : "
                     << file.errorString();
    }
  }
}

static QList<quint64> _noAuditInstanceIds { 0 };

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
  if (auditAction.contains(webconsole->showAuditEvent()) // empty regexps match any string
      && (webconsole->hideAuditEvent().pattern().isEmpty()
          || !auditAction.contains(webconsole->hideAuditEvent()))
      && userid.contains(webconsole->showAuditUser())
      && (webconsole->hideAuditUser().pattern().isEmpty()
          || !userid.contains(webconsole->hideAuditUser()))) {
    foreach (quint64 auditInstanceId, auditInstanceIds)
      Log::info(auditTaskId, auditInstanceId)
          << "AUDIT action: '" << auditAction
          << (responseMessage.startsWith('E')
              ? "' result: failure" : "' result: success")
          << " actor: '" << userid
          << "' address: { " << req.clientAdresses().join(", ")
          << " } params: " << req.paramsAsParamSet().toString(false)
          << " response message: " << responseMessage;
  }
  if (!redirect.isEmpty()) {
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
  foreach (QString key, params.keys())
    if (params.rawValue(key).isEmpty())
      params.removeValue(key);
  // LATER should check that mandatory form fields have been set ?
  TaskInstanceList instances = webconsole->scheduler()
      ->syncRequestTask(taskId, params);
  QString message;
  QList<quint64> taskInstanceIds;
  if (!instances.isEmpty()) {
    message = "S:Task '"+taskId+"' submitted for execution with id";
    foreach (TaskInstance request, instances) {
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
      webconsole->scheduler()->abortTaskInstancesByTaskId(taskId);
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
      webconsole->scheduler()->cancelRequestsByTaskId(taskId);
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
      webconsole->scheduler()->cancelRequestsByTaskId(taskId);
  message = "S:Task requests { "+instances.join(' ')
      +" } canceled and task instances { ";
  instances = webconsole->scheduler()->abortTaskInstancesByTaskId(taskId);
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
  TaskInstance instance = webconsole->scheduler()->abortTask(taskInstanceId);
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
      webconsole->scheduler()->cancelRequest(taskInstanceId);
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
      webconsole->scheduler()->cancelRequest(taskInstanceId);
  if (instance.isNull()) {
    instance = webconsole->scheduler()->abortTask(taskInstanceId);
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
  QString notice = req.url().path().mid(matchedLength);
  ParamSet params = req.paramsAsParamSet();
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
  QString alertid = req.url().path().mid(matchedLength);
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
  QString alertid = req.url().path().mid(matchedLength);
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
  QString alertid = req.url().path().mid(matchedLength);
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
  QString alertid = req.url().path().mid(matchedLength);
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
  QString alertid = req.url().path().mid(matchedLength);
  webconsole->scheduler()->alerter()->emitAlert(alertid);
  apiAuditAndResponse(webconsole, req, res, processingContext,
                      "S:Emitted alert '"+alertid+"'.",
                      req.methodName()+" "+req.url().path().left(matchedLength)
                      );
  return true;
}, true },
{ { "/console/do", "/rest/do" }, []( // LATER migrate to /do/v1 and remove
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int) {
  if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD|HttpRequest::POST
                      |HttpRequest::PUT, req, res))
    return true;
  QString event = req.param("event");
  QString taskId = req.param("taskid");
  QString alertId = req.param("alertid");
  QString gridboardId = req.param("gridboardid");
  if (alertId.isNull()) // LATER remove this backward compatibility trick
    alertId = req.param("alert");
  QString configId = req.param("configid");
  quint64 taskInstanceId = req.param("taskinstanceid").toULongLong();
  QList<quint64> auditInstanceIds;
  QString referer = req.header("Referer");
  QString redirect = req.base64Cookie("redirect", referer);
  QString message;
  QString userid = processingContext->paramValue("userid").toString();
  if (event == "requestTask") {
    // 192.168.79.76:8086/console/do?event=requestTask&taskid=appli.batch.batch1
    ParamSet params(req.paramsAsParamSet());
    // TODO handle null values rather than replacing empty with nulls
    foreach (QString key, params.keys())
      if (params.value(key).isEmpty())
        params.removeValue(key);
    params.removeValue("taskid");
    params.removeValue("event");
    // FIXME evaluate overriding params within overriding > global context
    TaskInstanceList instances = webconsole->scheduler()
        ->syncRequestTask(taskId, params);
    if (!instances.isEmpty()) {
      message = "S:Task '"+taskId+"' submitted for execution with id";
      foreach (TaskInstance request, instances) {
        message.append(' ').append(QString::number(request.idAsLong()));
        auditInstanceIds << request.idAsLong();
      }
      message.append('.');
    } else
      message = "E:Execution request of task '"+taskId
          +"' failed (see logs for more information).";
  } else if (event == "cancelRequest") {
    TaskInstance instance = webconsole->scheduler()
        ->cancelRequest(taskInstanceId);
    if (!instance.isNull()) {
      message = "S:Task request "+QString::number(taskInstanceId)
          +" canceled.";
      taskId = instance.task().id();
    } else
      message = "E:Cannot cancel request "+QString::number(taskInstanceId)
          +".";
  } else if (event == "abortTask") {
    TaskInstance instance = webconsole->scheduler()
        ->abortTask(taskInstanceId);
    if (!instance.isNull()) {
      message = "S:Task "+QString::number(taskInstanceId)+" aborted.";
      taskId = instance.task().id();
    } else
      message = "E:Cannot abort task "+QString::number(taskInstanceId)+".";
  } else if (event == "enableTask") {
    bool enable = req.param("enable") == "true";
    if (webconsole->scheduler()->enableTask(taskId, enable))
      message = "S:Task '"+taskId+"' "+(enable?"enabled":"disabled")+".";
    else
      message = "E:Task '"+taskId+"' not found.";
  } else if (event == "cancelAlert") {
    if (req.param("immediately") == "true")
      webconsole->scheduler()->alerter()->cancelAlertImmediately(alertId);
    else
      webconsole->scheduler()->alerter()->cancelAlert(alertId);
    message = "S:Canceled alert '"+alertId+"'.";
  } else if (event == "raiseAlert") {
    if (req.param("immediately") == "true")
      webconsole->scheduler()->alerter()->raiseAlertImmediately(alertId);
    else
      webconsole->scheduler()->alerter()->raiseAlert(alertId);
    message = "S:Raised alert '"+alertId+"'.";
  } else if (event == "emitAlert") {
    webconsole->scheduler()->alerter()->emitAlert(alertId);
    message = "S:Emitted alert '"+alertId+"'.";
  } else if (event == "clearGridboard") {
    webconsole->scheduler()->alerter()->clearGridboard(gridboardId);
    message = "S:Cleared gridboard '"+gridboardId+"'.";
  } else if (event=="enableAllTasks") {
    bool enable = req.param("enable") == "true";
    webconsole->scheduler()->enableAllTasks(enable);
    message = QString("S:Asked for ")+(enable?"enabling":"disabling")
        +" all tasks at once.";
    // wait to make it less probable that the page displays before effect
    QThread::usleep(500000);
  } else if (event=="reloadConfig") {
    // TODO should not display reload button when no config file is defined
    bool ok = Qrond::instance()->loadConfig();
    message = ok ? "S:Configuration reloaded."
                 : "E:Cannot reload configuration.";
    // wait to make it less probable that the page displays before effect
    QThread::usleep(1000000);
  } else if (event=="removeConfig") {
    bool ok = webconsole->configRepository()->removeConfig(configId);
    message = ok ? "S:Configuration removed."
                 : "E:Cannot remove configuration.";
  } else if (event=="activateConfig") {
    bool ok = webconsole->configRepository()->activateConfig(configId);
    message = ok ? "S:Configuration activated."
                 : "E:Cannot activate configuration.";
    // wait to make it less probable that the page displays before effect
    QThread::usleep(1000000);
  } else
    message = "E:Internal error: unknown event '"+event+"'.";
  if (message.startsWith("E:") || message.startsWith("W:"))
    res.setStatus(500); // LATER use more return codes
  if (event.contains(webconsole->showAuditEvent()) // empty regexps match any string
      && (webconsole->hideAuditEvent().pattern().isEmpty()
          || !event.contains(webconsole->hideAuditEvent()))
      && userid.contains(webconsole->showAuditUser())
      && (webconsole->hideAuditUser().pattern().isEmpty()
          || !userid.contains(webconsole->hideAuditUser()))) {
    if (auditInstanceIds.isEmpty())
      auditInstanceIds << taskInstanceId;
    foreach (quint64 auditInstanceId, auditInstanceIds)
      Log::info(taskId, auditInstanceId)
          << "AUDIT action: '" << event
          << ((res.status() < 300 && res.status() >=200)
              ? "' result: success" : "' result: failure")
          << " actor: '" << userid
          << "' address: { " << req.clientAdresses().join(", ")
          << " } params: " << req.paramsAsParamSet().toString(false)
          << " response message: " << message;
  }
  if (!redirect.isEmpty()) {
    res.setBase64SessionCookie("message", message, "/");
    res.clearCookie("redirect", "/");
    res.redirect(redirect);
  } else {
    res.setContentType("text/plain;charset=UTF-8");
    message.append("\n");
    res.output()->write(message.toUtf8());
  }
  return true;
} },
{ "/console/confirm", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD|HttpRequest::POST,
                          req, res))
        return true;
      QString event = req.param("event");
      QString taskId = req.param("taskid");
      QString gridboardId = req.param("gridboardid");
      QString taskInstanceId = req.param("taskinstanceid");
      QString configId = req.param("configid");
      QString referer = req.header("Referer", "overview.html");
      QString message;
      // LATER remove default "do" doUrl, port every call to /do/v1 API
      // LATER maybe also remove "/console/confirm" as a whole
      QString doPath = "do";
      QUrlQuery doQuery(req.url());
      // doQuery.removeAllQueryItems("event");
      if (event == "abortTask") {
        message = "abort task "+taskInstanceId;
        doPath = "../do/v1/taskinstances/abort/"+taskInstanceId;
        doQuery.clear();
      } else if (event == "cancelRequest") {
        message = "cancel request "+taskInstanceId;
        doPath = "../do/v1/taskinstances/cancel/"+taskInstanceId;
        doQuery.clear();
      } else if (event == "enableAllTasks") {
        message = QString(req.param("enable") == "true" ? "enable" : "disable")
            + " all tasks";
        doPath = QStringLiteral("../do/v1/tasks/")
                                +(req.param("enable") == "true" ? "enable_all" : "disable_all");
      } else if (event == "enableTask") {
        message = QString(req.param("enable") == "true" ? "enable" : "disable")
            + " task '"+taskId+"'";
      } else if (event == "reloadConfig") {
        message = "reload configuration";
      } else if (event == "removeConfig") {
        message = "remove configuration "+configId;
      } else if (event == "activateConfig") {
        message = "activate configuration "+configId;
      } else if (event == "clearGridboard") {
        message = "clear gridboard "+gridboardId;
      } else {
        message = event;
      }
      message = "<div class=\"well\">"
                "<h4 class=\"text-center\">Are you sure you want to "+message
          +" ?</h4><p><p class=\"text-center\"><a class=\"btn btn-danger\" "
           "href=\""+doPath+"?"+doQuery.toString(QUrl::FullyEncoded)
          +"\">Yes, sure</a> <a class=\"btn\" href=\""+referer
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
} },
{ "/console/tasks/request/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::POST|HttpRequest::HEAD,
                          req, res))
        return true;
      QString taskId = req.url().path().mid(matchedLength);
      QString referer = req.header("Referer", "../../overview.html");
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
        if (task.requestFormFields().size())
          form += "<p class=\"text-center\">Task parameters can be defined in "
                  "the following form:";
        form += "<p><form action=\"../../../do/v1/tasks/request/"+taskId
            +"\" method=\"POST\">";
        bool errorOccured = false;
        foreach (RequestFormField rff, task.requestFormFields())
            form.append(rff.toHtmlFormFragment(
                            webconsole->readOnlyResourcesCache(),
                            &errorOccured));
        form += "<div><p><p class=\"text-center\">"
                "<button type=\"submit\" class=\"btn ";
        if (errorOccured)
            form += "btn-default\" disabled><i class=\"icon-block\"></i> Cannot"
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
        Log::fatal() << "handleRequest adhoc from /tasks/request: " << processingContext->paramValue("!pathtoroot"); // FIXME
        webconsole->wuiHandler()->handleRequest(req, res, processingContext);
        return true;
      } else {
        res.setBase64SessionCookie("message", "E:Task '"+taskId+"' not found.",
                                   "/");
        res.redirect(referer);
      }
      return true;
}, true },
{ "/console/taskdoc.html", []( // LATER remove
    WebConsole *, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      res.redirect("tasks/"+req.param("taskid"));
      return true;
} },
{ "/console/tasks/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      CharacterSeparatedExpression elements(req.url().path(), matchedLength-1);
      QString taskId = elements.value(0);
      QString referer = req.header("Referer", "../overview.html");
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
                task.toPfNode().toPf(PfOptions().setShouldIndent()
                                     .setShouldWriteContentBeforeSubnodes()
                                     .setShouldIgnoreComment(false)))));
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
    ParamsProviderMerger *processingContext, int matchedLength) {
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
      if (subItem == "workflow.svg") {
        // LATER this rendering should be pooled
        QString gv = task.graphvizWorkflowDiagram();
        if (!gv.isEmpty()) {
          GraphvizImageHttpHandler *h = new GraphvizImageHttpHandler;
          h->setImageFormat(GraphvizImageHttpHandler::Svg);
          h->setSource(gv);
          h->handleRequest(req, res, processingContext);
          h->deleteLater(); // LATER why deleting *later* ?
          return true;
        }
        if (!task.workflowTaskId().isEmpty()) {
          return writeSvgImage(QString(SVG_BELONG_TO_WORKFLOW)
                               .arg(task.workflowTaskId()).toUtf8(), req, res);
        }
        return writeSvgImage(SVG_NOT_A_WORKFLOW, req, res);
      }
      if (subItem == "workflow.dot") {
        QString gv = task.graphvizWorkflowDiagram();
        if (!gv.isEmpty())
          return writePlainText(gv.toUtf8(), req, res, GRAPHVIZ_MIME_TYPE);
        res.setStatus(404);
        res.output()->write("No such workflow.");
        return true;
      }
      if (subItem == "config.pf") {
        return writePlainText(task.toPfNode().toPf(
                                PfOptions().setShouldIndent()
                                .setShouldWriteContentBeforeSubnodes()
                                .setShouldIgnoreComment(false)), req, res);
      }
      return false;
}, true },
{ "/console/gridboard.html", []( // LATER remove
    WebConsole *, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      res.redirect("gridboards/"+req.param("gridboardid"));
      return true;
} },
{ "/console/gridboards/", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext, int matchedLength) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::POST|HttpRequest::HEAD,
                          req, res))
        return true;
      QString gridboardId = req.url().path().mid(matchedLength);
      QString referer = req.header("Referer", "overview.html");
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
      if (queryItems.size()) {
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
        int i = s.lastIndexOf('/');
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
      // LATER handle fields comma enumeration for real
      QString fields = req.param(QStringLiteral("fields")).trimmed();
      // LATER return writeCsvView(webconsole->csvTaskGroupsEventsView(), req, res);
      return writeCsvView(webconsole->csvTaskGroupsView(), req, res);
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
      // LATER handle fields comma enumeration for real
      QString fields = req.param(QStringLiteral("fields")).trimmed();
      // LATER if (fields == QStringLiteral("id,triggers,onstart,onsuccess,onfailure"))
      //  return writeHtmlView(webconsole->csvTasksEventsView(), req, res);
      return writeCsvView(webconsole->csvTasksView(), req, res);
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
{ "/rest/v1/steps/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvStepsView(), req, res);
} },
{ "/rest/v1/steps/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlStepsView(), req, res);
} },
{ "/rest/v1/hosts/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvHostsListView(), req, res);
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
      return writeCsvView(webconsole->csvClustersListView(), req, res);
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
{ "/rest/v1/global_setenvs/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvGlobalSetenvView(), req, res);
} },
{ "/rest/v1/global_setenvs/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlGlobalSetenvView(), req, res);
} },
{ "/rest/v1/global_unsetenvs/list.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvGlobalUnsetenvView(), req, res);
} },
{ "/rest/v1/global_unsetenvs/list.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlGlobalUnsetenvView(), req, res);
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
      return writeCsvView(webconsole->csvAlertSubscriptionsView(), req, res);
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
      return writeCsvView(webconsole->csvAlertSettingsView(), req, res);
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
      return writeCsvView(webconsole->csvGridboardsView(), req, res);
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
          } else
            res.setContentType("text/plain;charset=UTF-8");
            config.toPfNode() // LATER remove indentation
                .writePf(res.output(), PfOptions().setShouldIndent()
                         .setShouldWriteContentBeforeSubnodes()
                         .setShouldIgnoreComment(false));
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
      return writeCsvView(webconsole->csvLogFilesView(), req, res);
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
      return writeCsvView(webconsole->csvCalendarsView(), req, res);
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
{ "/rest/v1/logs/last_info_entries.csv", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeCsvView(webconsole->csvLogView(), req, res);
} },
{ "/rest/v1/logs/last_info_entries.html", [](
    WebConsole *webconsole, HttpRequest req, HttpResponse res,
    ParamsProviderMerger *, int) {
      if (!enforceMethods(HttpRequest::GET|HttpRequest::HEAD, req, res))
        return true;
      return writeHtmlView(webconsole->htmlInfoLogView(), req, res);
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
};

RadixTree<QString> _staticRedirects {
  { { "/", "/console" }, "console/overview.html" },
  { { "/console/", "/console/index.html"}, "overview.html" },
  { "/console/infra.html", "infrastructure.html" }, // LATER remove
  { "/console/log.html", "logs.html" },  // LATER remove
};

bool WebConsole::handleRequest(
    HttpRequest req, HttpResponse res,
    ParamsProviderMerger *processingContext) {
  if (redirectForUrlCleanup(req, res, processingContext))
    return true;
  QString path = req.url().path();
  ParamsProviderMergerRestorer restorer(processingContext);
  WebConsoleParamsProvider webconsoleParams(this);
  processingContext->append(&webconsoleParams);
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
  processingContext->append(_scheduler->globalParams());
  QString userid = processingContext->paramValue("userid").toString();
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
    _globalSetenvModel->connectToDocumentManager<QronConfigDocumentManager>(
          _scheduler, _scheduler->globalSetenvs(), "globalsetenvs",
          "globalsetenv", &QronConfigDocumentManager::paramsChanged,
          &QronConfigDocumentManager::changeParams);
    _globalUnsetenvModel->connectToDocumentManager<QronConfigDocumentManager>(
          _scheduler, _scheduler->globalUnsetenvs(), "globalunsetenvs",
          "globalunsetenv", &QronConfigDocumentManager::paramsChanged,
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
    _unfinishedTaskInstancetModel->setDocumentManager(scheduler);
    _calendarsModel->setDocumentManager(scheduler);
    _taskGroupsModel->setDocumentManager(scheduler);
    _stepsModel->setDocumentManager(scheduler);
    connect(_scheduler, &Scheduler::globalEventSubscriptionsChanged,
            _schedulerEventsModel, &SchedulerEventsModel::globalEventSubscriptionsChanged);
    connect(_scheduler, SIGNAL(noticePosted(QString,ParamSet)),
            _lastPostedNoticesModel, SLOT(eventOccured(QString)));
    connect(_scheduler, &Scheduler::accessControlConfigurationChanged,
            this, &WebConsole::enableAccessControl);
    connect(_scheduler, &Scheduler::logConfigurationChanged,
            _logConfigurationModel, &LogFilesModel::logConfigurationChanged);
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
        .allow("", "", "^/console/(css|jsp|js|img)/")
        // anyone for test page and user manual
        .allow("", "", "^/console/(test|user-manual)\\.html$")
        // read for read-only rest calls
        .allow("read", "^GET|HEAD$", "^/rest/")
        // operate for operation including other rest calls
        .allow("operate", "", "^/(rest|console)/(do|confirm)") // LATER keep only /console/confirm/
        .allow("operate", "", "^/console/tasks/request/")
        .allow("operate", "", "^/do/")
        .allow("operate", "", "^/rest/")
        // nobody else on operation and rest paths
        .deny("", "", "^/(rest|console)/(do|confirm)") // LATER keep only /console/confirm/
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
  _unfinishedTaskInstancetModel->setCustomActions(customactions_instanceslist);
  _taskInstancesHistoryModel->setCustomActions(customactions_instanceslist);
  int rowsPerPage = newParams.valueAsInt(
        "webconsole.htmltables.rowsperpage", 100);
  int cachedRows = newParams.valueAsInt(
        "webconsole.htmltables.cachedrows", 500);
  foreach (QObject *child, children()) {
    auto *htmlView = qobject_cast<HtmlTableView*>(child);
    auto *csvView = qobject_cast<CsvTableView*>(child);
    if (htmlView) {
      if (htmlView != _htmlWarningLogView10
          && htmlView != _htmlTaskInstancesView20
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
}

void WebConsole::alerterConfigChanged(AlerterConfig config) {
  _alerterConfig = config;
  _alertSubscriptionsModel->setItems(config.alertSubscriptions());
  _alertSettingsModel->setItems(config.alertSettings());
  _alertChannelsModel->clear();
  _gridboardsModel->setItems((config.gridboards()));
  foreach (const QString channel, config.channelsNames())
    _alertChannelsModel->setCellValue(channel, "enabled", "true");
}
