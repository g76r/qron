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

WebConsole::WebConsole() : _thread(new QThread), _scheduler(0),
  _tasksTreeModel(new TasksTreeModel(this)),
  _targetsTreeModel(new TargetsTreeModel(this)),
  _hostsListModel(new HostsListModel(this)),
  _clustersListModel(new ClustersListModel(this)),
  _resourceAllocationModel(new ResourcesAllocationModel(this)),
  _globalParamsModel(new ParamSetModel(this)),
  _alertParamsModel(new ParamSetModel(this)),
  _raisedAlertsModel(new RaisedAlertsModel(this)),
  _lastEmitedAlertsModel(new LastEmitedAlertsModel(this, 50)),
  _lastPostedNoticesModel(new LastOccuredTextEventsModel(this, 50)),
  _lastFlagsChangesModel(new LastOccuredTextEventsModel(this, 50)),
  _alertRulesModel(new AlertRulesModel(this)),
  // memory cost: about 1.5 kB / request, e.g. 30 MB for 20000 requests
  // (this is an empirical measurement and thus includes model + csv view
  _taskRequestsHistoryModel(new TaskRequestsModel(this, 20000)),
  _unfinishedTaskRequestModel(new TaskRequestsModel(this, 20, false)),
  _tasksModel(new TasksModel(this)),
  _schedulerEventsModel(new SchedulerEventsModel(this)),
  _flagsSetModel(new FlagsSetModel(this)),
  _taskGroupsModel(new TaskGroupsModel(this)),
  _alertChannelsModel(new AlertChannelsModel(this)),
  _htmlTasksTreeView(new HtmlTreeView(this)),
  _htmlTargetsTreeView(new HtmlTreeView(this)),
  _htmlHostsListView(new HtmlTableView(this)),
  _htmlClustersListView(new HtmlTableView(this)),
  _htmlResourcesAllocationView(new HtmlTableView(this)),
  _htmlGlobalParamsView(new HtmlTableView(this)),
  _htmlAlertParamsView(new HtmlTableView(this)),
  _htmlRaisedAlertsView(new HtmlTableView(this)),
  _htmlRaisedAlertsView10(new HtmlTableView(this)),
  _htmlLastEmitedAlertsView(new HtmlTableView(this)),
  _htmlLastEmitedAlertsView10(new HtmlTableView(this)),
  _htmlAlertRulesView(new HtmlTableView(this)),
  _htmlWarningLogView(new HtmlTableView(this)),
  _htmlWarningLogView10(new HtmlTableView(this)),
  _htmlInfoLogView(new HtmlTableView(this)),
  _htmlTaskRequestsView(new HtmlTableView(this)),
  _htmlTaskRequestsView20(new HtmlTableView(this)),
  _htmlTasksScheduleView(new HtmlTableView(this)),
  _htmlTasksConfigView(new HtmlTableView(this)),
  _htmlTasksParamsView(new HtmlTableView(this)),
  _htmlTasksListView(new HtmlTableView(this)),
  _htmlTasksEventsView(new HtmlTableView(this)),
  _htmlSchedulerEventsView(new HtmlTableView(this)),
  _htmlLastPostedNoticesView20(new HtmlTableView(this)),
  _htmlLastFlagsChangesView20(new HtmlTableView(this)),
  _htmlFlagsSetView20(new HtmlTableView(this)),
  _htmlTaskGroupsView(new HtmlTableView(this)),
  _htmlTaskGroupsEventsView(new HtmlTableView(this)),
  _htmlAlertChannelsView(new HtmlTableView(this)),
  _htmlTasksResourcesView(new HtmlTableView(this)),
  _clockView(new ClockView(this)),
  _csvTasksTreeView(new CsvTableView(this)),
  _csvTargetsTreeView(new CsvTableView(this)),
  _csvHostsListView(new CsvTableView(this)),
  _csvClustersListView(new CsvTableView(this)),
  _csvResourceAllocationView(new CsvTableView(this)),
  _csvGlobalParamsView(new CsvTableView(this)),
  _csvAlertParamsView(new CsvTableView(this)),
  _csvRaisedAlertsView(new CsvTableView(this)),
  _csvLastEmitedAlertsView(new CsvTableView(this)),
  _csvAlertRulesView(new CsvTableView(this)),
  _csvLogView(new CsvTableView(this)),
  _csvTaskRequestsView(new CsvTableView(this)),
  _csvTasksView(new CsvTableView(this)),
  _csvSchedulerEventsView(new CsvTableView(this)),
  _csvLastPostedNoticesView(new CsvTableView(this)),
  _csvLastFlagsChangesView(new CsvTableView(this)),
  _csvFlagsSetView(new CsvTableView(this)),
  _csvTaskGroupsView(new CsvTableView(this)),
  _wuiHandler(new TemplatingHttpHandler(this, "/console", ":docroot/console")),
  _memoryInfoLogger(new MemoryLogger(0, Log::Info, 200)),
  _memoryWarningLogger(new MemoryLogger(0, Log::Warning, 100)),
  _title("Qron Web Console"), _navtitle("Qron Web Console") {
  _thread->setObjectName("WebConsoleServer");
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  _htmlTasksTreeView->setModel(_tasksTreeModel);
  _htmlTasksTreeView->setTableClass("table table-condensed table-hover");
  _htmlTasksTreeView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTargetsTreeView->setModel(_targetsTreeModel);
  _htmlTargetsTreeView->setTableClass("table table-condensed table-hover");
  _htmlTargetsTreeView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlHostsListView->setModel(_hostsListModel);
  _htmlHostsListView->setTableClass("table table-condensed table-hover");
  _htmlHostsListView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlHostsListView->setEmptyPlaceholder("(no host)");
  _htmlClustersListView->setModel(_clustersListModel);
  _htmlClustersListView->setTableClass("table table-condensed table-hover");
  _htmlClustersListView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlClustersListView->setEmptyPlaceholder("(no cluster)");
  _htmlResourcesAllocationView->setModel(_resourceAllocationModel);
  _htmlResourcesAllocationView->setTableClass("table table-condensed "
                                             "table-hover table-bordered");
  _htmlResourcesAllocationView->setRowHeaders();
  _htmlResourcesAllocationView->setEmptyPlaceholder("(no resource definition)");
  _htmlResourcesAllocationView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlGlobalParamsView->setModel(_globalParamsModel);
  _htmlGlobalParamsView->setTableClass("table table-condensed table-hover");
  _htmlAlertParamsView->setModel(_alertParamsModel);
  _htmlAlertParamsView->setTableClass("table table-condensed table-hover");
  _raisedAlertsModel->setPrefix("<i class=\"icon-bell\"></i> ",
                                TextViews::HtmlPrefixRole);
  _htmlRaisedAlertsView->setModel(_raisedAlertsModel);
  _htmlRaisedAlertsView->setTableClass("table table-condensed table-hover");
  _htmlRaisedAlertsView->setEmptyPlaceholder("(no alert)");
  _htmlRaisedAlertsView
      ->setEllipsePlaceholder("(alerts list too long to be displayed)");
  _htmlRaisedAlertsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlRaisedAlertsView10->setModel(_raisedAlertsModel);
  _htmlRaisedAlertsView10->setTableClass("table table-condensed table-hover");
  _htmlRaisedAlertsView10->setEmptyPlaceholder("(no alert)");
  _htmlRaisedAlertsView10
      ->setEllipsePlaceholder("(see alerts page for more alerts)");
  _htmlRaisedAlertsView10->setMaxrows(10);
  _htmlRaisedAlertsView10->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _lastEmitedAlertsModel->setEventName("Alert");
  _lastEmitedAlertsModel->setPrefix("<i class=\"icon-bell\"></i> ", 0);
  _lastEmitedAlertsModel->setPrefix("<i class=\"icon-ok\"></i> ", 1);
  _lastEmitedAlertsModel->setPrefixRole(TextViews::HtmlPrefixRole);
  _htmlLastEmitedAlertsView->setModel(_lastEmitedAlertsModel);
  _htmlLastEmitedAlertsView->setTableClass("table table-condensed table-hover");
  _htmlLastEmitedAlertsView->setMaxrows(50);
  _htmlLastEmitedAlertsView->setEmptyPlaceholder("(no alert)");
  _htmlLastEmitedAlertsView
      ->setEllipsePlaceholder("(alerts list too long to be displayed)");
  _htmlLastEmitedAlertsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlLastEmitedAlertsView10->setModel(_lastEmitedAlertsModel);
  _htmlLastEmitedAlertsView10
      ->setTableClass("table table-condensed table-hover");
  _htmlLastEmitedAlertsView10->setMaxrows(10);
  _htmlLastEmitedAlertsView10->setEmptyPlaceholder("(no alert)");
  _htmlLastEmitedAlertsView10
      ->setEllipsePlaceholder("(see alerts page for more alerts)");
  _htmlLastEmitedAlertsView10->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlAlertRulesView->setModel(_alertRulesModel);
  _htmlAlertRulesView->setTableClass("table table-condensed table-hover");
  _htmlAlertRulesView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlWarningLogView->setModel(_memoryWarningLogger->model());
  _htmlWarningLogView->setTableClass("table table-condensed table-hover");
  _htmlWarningLogView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlWarningLogView->setTrClassRole(LogModel::TrClassRole);
  _htmlWarningLogView
      ->setEllipsePlaceholder("(download text log file for more entries)");
  _htmlWarningLogView->setEmptyPlaceholder("(empty log)");
  _htmlWarningLogView10->setModel(_memoryWarningLogger->model());
  _htmlWarningLogView10->setTableClass("table table-condensed table-hover");
  _htmlWarningLogView10->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlWarningLogView10->setTrClassRole(LogModel::TrClassRole);
  _htmlWarningLogView10->setMaxrows(10);
  _htmlWarningLogView10->setEllipsePlaceholder("(see log page for more entries)");
  _htmlWarningLogView10->setEmptyPlaceholder("(empty log)");
  _htmlInfoLogView->setModel(_memoryInfoLogger->model());
  _htmlInfoLogView->setTableClass("table table-condensed table-hover");
  _htmlInfoLogView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlInfoLogView->setTrClassRole(LogModel::TrClassRole);
  _htmlInfoLogView
      ->setEllipsePlaceholder("(download text log file for more entries)");
  _htmlWarningLogView->setEmptyPlaceholder("(empty log)");
  _htmlTaskRequestsView20->setModel(_unfinishedTaskRequestModel);
  _htmlTaskRequestsView20->setTableClass("table table-condensed table-hover");
  _htmlTaskRequestsView20->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTaskRequestsView20->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTaskRequestsView20->setTrClassRole(LogModel::TrClassRole);
  _htmlTaskRequestsView20->setMaxrows(20);
  _htmlTaskRequestsView20
      ->setEllipsePlaceholder("(see tasks page for more entries)");
  _htmlTaskRequestsView20->setEmptyPlaceholder("(no running or queued task)");
  _htmlTaskRequestsView->setModel(_taskRequestsHistoryModel);
  _htmlTaskRequestsView->setTableClass("table table-condensed table-hover");
  _htmlTaskRequestsView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTaskRequestsView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTaskRequestsView->setTrClassRole(LogModel::TrClassRole);
  _htmlTaskRequestsView->setMaxrows(100);
  _htmlTaskRequestsView
      ->setEllipsePlaceholder("(tasks list too long to be displayed)");
  _htmlTaskRequestsView->setEmptyPlaceholder("(no recent task)");
  _htmlTasksScheduleView->setModel(_tasksModel);
  _htmlTasksScheduleView->setTableClass("table table-condensed table-hover");
  _htmlTasksScheduleView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlTasksScheduleView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksScheduleView->setEmptyPlaceholder("(no task in configuration)");
  QList<int> cols;
  cols << 11 << 2 << 5 << 6 << 19 << 10 << 17 << 18;
  _htmlTasksScheduleView->setColumnIndexes(cols);
  _htmlTasksConfigView->setModel(_tasksModel);
  _htmlTasksConfigView->setTableClass("table table-condensed table-hover");
  _htmlTasksConfigView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlTasksConfigView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksConfigView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 1 << 0 << 3 << 5 << 4 << 6 << 8 << 12;
  _htmlTasksConfigView->setColumnIndexes(cols);
  _htmlTasksParamsView->setModel(_tasksModel);
  _htmlTasksParamsView->setTableClass("table table-condensed table-hover");
  _htmlTasksParamsView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlTasksParamsView->setHtmlSuffixRole(TextViews::HtmlSuffixRole);
  _htmlTasksParamsView->setEmptyPlaceholder("(no task in configuration)");
  cols.clear();
  cols << 1 << 0 << 7 << 21 << 22;
  _htmlTasksParamsView->setColumnIndexes(cols);
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
  _htmlLastPostedNoticesView20->setMaxrows(20);
  _htmlLastPostedNoticesView20->setEmptyPlaceholder("(no notice)");
  _htmlLastPostedNoticesView20
      ->setEllipsePlaceholder("(older notices not displayed)");
  _htmlLastPostedNoticesView20->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _lastFlagsChangesModel->setEventName("Flag change");
  _lastFlagsChangesModel->setPrefix("<i class=\"icon-flag\"></i> ", 0);
  _lastFlagsChangesModel->setPrefix("<i class=\"icon-minus\"></i> ", 1);
  _lastFlagsChangesModel->setPrefixRole(TextViews::HtmlPrefixRole);
  _htmlLastFlagsChangesView20->setModel(_lastFlagsChangesModel);
  _htmlLastFlagsChangesView20
      ->setTableClass("table table-condensed table-hover");
  _htmlLastFlagsChangesView20->setMaxrows(20);
  _htmlLastFlagsChangesView20->setEmptyPlaceholder("(no flags changes)");
  _htmlLastFlagsChangesView20
      ->setEllipsePlaceholder("(older changes not displayed)");
  _htmlLastFlagsChangesView20->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _flagsSetModel->setPrefix("<i class=\"icon-flag\"></i> ",
                            TextViews::HtmlPrefixRole);
  _htmlFlagsSetView20->setModel(_flagsSetModel);
  _htmlFlagsSetView20->setTableClass("table table-condensed table-hover");
  _htmlFlagsSetView20->setMaxrows(20);
  _htmlFlagsSetView20->setEmptyPlaceholder("(no flags set)");
  _htmlFlagsSetView20->setEllipsePlaceholder("(more flags not displayed)");
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
  cols << 11 << 12 << 8;
  _htmlTasksResourcesView->setColumnIndexes(cols);
  _clockView->setFormat("yyyy-MM-dd hh:mm:ss,zzz");
  _csvTasksTreeView->setModel(_tasksTreeModel);
  _csvTasksTreeView->setFieldQuote('"');
  _csvTargetsTreeView->setModel(_targetsTreeModel);
  _csvTargetsTreeView->setFieldQuote('"');
  _csvHostsListView->setModel(_hostsListModel);
  _csvHostsListView->setFieldQuote('"');
  _csvClustersListView->setModel(_clustersListModel);
  _csvClustersListView->setFieldQuote('"');
  _csvResourceAllocationView->setModel(_resourceAllocationModel);
  _csvResourceAllocationView->setFieldQuote('"');
  _csvResourceAllocationView->setRowHeaders();
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
  _csvTaskRequestsView->setMaxrows(20000);
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
  _wuiHandler->addView("taskstree", _htmlTasksTreeView);
  _wuiHandler->addView("targetstree", _htmlTargetsTreeView);
  _wuiHandler->addView("resourcesallocation", _htmlResourcesAllocationView);
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
  _memoryInfoLogger->moveToThread(_thread);
  _memoryWarningLogger->moveToThread(_thread);
}

QString WebConsole::name() const {
  return "WebConsole";
}

bool WebConsole::acceptRequest(HttpRequest req) {
  Q_UNUSED(req)
  return true;
}

void WebConsole::handleRequest(HttpRequest req, HttpResponse res) {
  QString path = req.url().path();
  while (path.size() && path.at(path.size()-1) == '/')
    path.chop(1);
  if (path.isEmpty()) {
    res.redirect("console/index.html");
    return;
  }
  if (path == "/console/do" || path == "/rest/do" ) {
    QString event = req.param("event");
    QString fqtn = req.param("fqtn");
    QString alert = req.param("alert");
    quint64 id = req.param("id").toULongLong();
    QString message;
    if (_scheduler) {
      if (event == "requestTask") {
        // 192.168.79.76:8086/console/do?event=requestTask&fqtn=appli.batch.batch1
        TaskRequest request = _scheduler->syncRequestTask(fqtn);
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
      } else
        message = "E:Internal error: unknown event '"+event+"'.";
    } else
      message = "E:Scheduler is not available.";
    QString referer = req.header("Referer");
    if (!referer.isEmpty()) {
      res.setBase64SessionCookie("message", message, "/");
      res.redirect(referer);
    } else {
      if (message.startsWith("E:") || message.startsWith("W:"))
        res.setStatus(500);
      res.setContentType("text/plain;charset=UTF-8");
      message.append("\n");
      res.output()->write(message.toUtf8());
    }
    return;
  }
  if (path.startsWith("/console")) {
    QHash<QString,QVariant> values;
    values.insert("title", _title);
    values.insert("navtitle", _navtitle);
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
      values.insert("message", "<div class=\"alert alert-block "+alertClass+"\">"
                    "<button type=\"button\" class=\"close\" "
                    "data-dismiss=\"alert\">&times;</button>"
                    "<h4>"+title+"</h4> "+message+"</div>");
    } else
      values.insert("message", "");
    res.clearCookie("message", "/");
    _wuiHandler->handleRequestWithContext(req, res, values);
    return;
  }
  // LATER optimize resource selection (avoid if/if/if)
  if (path == "/rest/csv/tasks/tree/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvTasksTreeView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/tasks/tree/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTasksTreeView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/tasks/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvTasksView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/tasks/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTasksListView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/tasks/events/v1") { // FIXME
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTasksEventsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/hosts/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvHostsListView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/hosts/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlHostsListView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/clusters/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvClustersListView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/clusters/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlClustersListView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/resources/allocation/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvResourceAllocationView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/resources/allocation/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlResourcesAllocationView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/params/global/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvGlobalParamsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/params/global/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlGlobalParamsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/alerts/params/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvAlertParamsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/alerts/params/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlAlertParamsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/alerts/raised/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvRaisedAlertsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/alerts/raised/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlRaisedAlertsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/alerts/emited/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLastEmitedAlertsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/alerts/emited/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlLastEmitedAlertsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/alerts/rules/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvAlertRulesView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/alerts/rules/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlAlertRulesView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/log/info/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLogView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/log/info/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlInfoLogView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/txt/log/current/v1") {
    QString path(Log::pathToLastFullestLog());
    if (path.isEmpty()) {
      res.setStatus(404);
      res.output()->write("No log file found.");
    } else {
      QFile file(path);
      if (file.open(QIODevice::ReadOnly)) {
        const QString filter = req.param("filter");
        res.setContentType("text/plain;charset=UTF-8");
        if (filter.isEmpty())
          IOUtils::copy(res.output(), &file, 100*1024*1024);
        else
          IOUtils::grepString(res.output(), &file, 100*1024*1024, filter);
      } else {
        Log::warning() << "web console cannot open log file " << path
                       << " : error #" << file.error() << " : "
                       << file.errorString();
        int status = file.error() == QFile::PermissionsError ? 403 : 404;
        res.setStatus(status);
        res.output()->write(status == 403 ? "Permission denied."
                                          : "Document not found.");
      }
    }
    return;
  }
  if (path == "/rest/txt/log/all/v1") {
    QStringList paths(Log::pathsToFullestLogs());
    if (paths.isEmpty()) {
      res.setStatus(404);
      res.output()->write("No log file found.");
    } else {
      res.setContentType("text/plain;charset=UTF-8");
      const QString filter = req.param("filter");
      foreach (const QString path, paths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
          if (filter.isEmpty())
            IOUtils::copy(res.output(), &file, 100*1024*1024);
          else
            IOUtils::grepString(res.output(), &file, 100*1024*1024, filter);
        } else {
          Log::warning() << "web console cannot open log file " << path
                         << " : error #" << file.error() << " : "
                         << file.errorString();
        }
      }
    }
    return;
  }
  if (path == "/rest/csv/taskrequests/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvTaskRequestsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/taskrequests/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTaskRequestsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/scheduler/events/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvSchedulerEventsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/scheduler/events/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlSchedulerEventsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/notices/lastposted/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLastPostedNoticesView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/notices/lastposted/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlLastPostedNoticesView20->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/flags/lastchanges/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvLastFlagsChangesView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/flags/lastchanges/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlLastFlagsChangesView20->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/flags/set/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvFlagsSetView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/flags/set/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlFlagsSetView20->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/csv/taskgroups/list/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.setHeader("Content-Disposition", "attachment; filename=table.csv");
    res.output()->write(_csvTaskGroupsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/taskgroups/list/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTaskGroupsView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/taskgroups/events/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTaskGroupsEventsView->text().toUtf8().constData());
    return;
  }
  res.setStatus(404);
  res.output()->write("Not found.");
}

void WebConsole::setScheduler(Scheduler *scheduler) {
  if (_scheduler) {
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
               _tasksTreeModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
               _targetsTreeModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
               _tasksModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
               _hostsListModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
               _clustersListModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    disconnect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QMap<QString,QMap<QString,qint64> >)),
               _resourceAllocationModel, SLOT(setResourceConfiguration(QMap<QString,QMap<QString,qint64> >)));
    disconnect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QMap<QString,qint64>)),
               _resourceAllocationModel, SLOT(setResourceAllocationForHost(QString,QMap<QString,qint64>)));
    disconnect(_scheduler, SIGNAL(taskQueued(TaskRequest)),
               _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskStarted(TaskRequest)),
               _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
               _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
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
    disconnect(_scheduler, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
               _taskRequestsHistoryModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskQueued(TaskRequest)),
               _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskStarted(TaskRequest)),
               _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
               _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)),
               _schedulerEventsModel, SLOT(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)));
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
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
               _taskGroupsModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    disconnect(_scheduler, SIGNAL(globalParamsChanged(ParamSet)),
               this, SLOT(globalParamsChanged(ParamSet)));
    disconnect(_scheduler->alerter(), SIGNAL(channelsChanged(QStringList)),
               _alertChannelsModel, SLOT(channelsChanged(QStringList)));
  }
  _scheduler = scheduler;
  if (_scheduler) {
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
            _tasksTreeModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
            _targetsTreeModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
            _tasksModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
            _hostsListModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
            _clustersListModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    connect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QMap<QString,QMap<QString,qint64> >)),
            _resourceAllocationModel, SLOT(setResourceConfiguration(QMap<QString,QMap<QString,qint64> >)));
    connect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QMap<QString,qint64>)),
            _resourceAllocationModel, SLOT(setResourceAllocationForHost(QString,QMap<QString,qint64>)));
    connect(_scheduler, SIGNAL(taskQueued(TaskRequest)),
            _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskStarted(TaskRequest)),
            _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
            _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
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
    connect(_scheduler, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
            _taskRequestsHistoryModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskQueued(TaskRequest)),
            _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskStarted(TaskRequest)),
            _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskFinished(TaskRequest,QWeakPointer<Executor>)),
            _unfinishedTaskRequestModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)),
            _schedulerEventsModel, SLOT(eventsConfigurationReset(QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>,QList<Event>)));
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
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
            _taskGroupsModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    connect(_scheduler, SIGNAL(globalParamsChanged(ParamSet)),
            this, SLOT(globalParamsChanged(ParamSet)));
    connect(_scheduler->alerter(), SIGNAL(channelsChanged(QStringList)),
            _alertChannelsModel, SLOT(channelsChanged(QStringList)));
    Log::addLogger(_memoryWarningLogger, false);
    Log::addLogger(_memoryInfoLogger, false);
  } else {
    _title = "Qron Web Console";
  }
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
  _title = globalParams.value("webconsole.title", "Qron Web Console");
  _navtitle = globalParams.value("webconsole.navtitle", _title);
}
