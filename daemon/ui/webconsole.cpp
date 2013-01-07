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

WebConsole::WebConsole() : _scheduler(0),
  _tasksTreeModel(new TasksTreeModel(this)),
  _targetsTreeModel(new TargetsTreeModel(this)),
  _hostsListModel(new HostsListModel(this)),
  _clustersListModel(new ClustersListModel(this)),
  _resourceAllocationModel(new ResourcesAllocationModel(this)),
  _globalParamsModel(new ParamSetModel(this)),
  _alertParamsModel(new ParamSetModel(this)),
  _raisedAlertsModel(new RaisedAlertsModel(this)),
  _lastEmitedAlertsModel(new LastEmitedAlertsModel(this, 51)),
  _alertRulesModel(new AlertRulesModel(this)),
  _htmlTasksTreeView(new HtmlTableView(this)),
  _htmlTargetsTreeView(new HtmlTableView(this)),
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
  _htmlLogView(new HtmlTableView(this)),
  _htmlLogView10(new HtmlTableView(this)),
  _clockView(new ClockView(this)),
  _csvTasksTreeView(new CsvView(this)),
  _csvTargetsTreeView(new CsvView(this)),
  _csvHostsListView(new CsvView(this)),
  _csvClustersListView(new CsvView(this)),
  _csvResourceAllocationView(new CsvView(this)),
  _csvGlobalParamsView(new CsvView(this)),
  _csvAlertParamsView(new CsvView(this)),
  _csvRaisedAlertsView(new CsvView(this)),
  _csvLastEmitedAlertsView(new CsvView(this)),
  _csvAlertRulesView(new CsvView(this)),
  _csvLogView(new CsvView(this)),
  _wuiHandler(new TemplatingHttpHandler(this, "/console", ":docroot/console")),
  _memoryLogger(new MemoryLogger) {
  _htmlTasksTreeView->setModel(_tasksTreeModel);
  _htmlTasksTreeView->setTableClass("table table-condensed table-hover");
  _htmlTasksTreeView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlTargetsTreeView->setModel(_targetsTreeModel);
  _htmlTargetsTreeView->setTableClass("table table-condensed table-hover");
  _htmlTargetsTreeView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlHostsListView->setModel(_hostsListModel);
  _htmlHostsListView->setTableClass("table table-condensed table-hover");
  _htmlHostsListView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlClustersListView->setModel(_clustersListModel);
  _htmlClustersListView->setTableClass("table table-condensed table-hover");
  _htmlClustersListView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlResourcesAllocationView->setModel(_resourceAllocationModel);
  _htmlResourcesAllocationView->setTableClass("table table-condensed "
                                             "table-hover table-bordered");
  _htmlResourcesAllocationView->setRowHeaders();
  //_htmlResourceAllocationView->setTopLeftHeader(QString::fromUtf8("Host × Resource"));
  _htmlResourcesAllocationView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlGlobalParamsView->setModel(_globalParamsModel);
  _htmlGlobalParamsView->setTableClass("table table-condensed table-hover");
  _htmlAlertParamsView->setModel(_alertParamsModel);
  _htmlAlertParamsView->setTableClass("table table-condensed table-hover");
  _htmlRaisedAlertsView->setModel(_raisedAlertsModel);
  _htmlRaisedAlertsView->setTableClass("table table-condensed table-hover");
  _htmlRaisedAlertsView->setEmptyPlaceholder("(no alert)");
  _htmlRaisedAlertsView
      ->setEllipsePlaceholder("(alerts list too long to be displayed)");
  _htmlRaisedAlertsView10->setModel(_raisedAlertsModel);
  _htmlRaisedAlertsView10->setTableClass("table table-condensed table-hover");
  _htmlRaisedAlertsView10->setEmptyPlaceholder("(no alert)");
  _htmlRaisedAlertsView10
      ->setEllipsePlaceholder("(see alerts page for more alerts)");
  _htmlRaisedAlertsView10->setMaxrows(10);
  _htmlLastEmitedAlertsView->setModel(_lastEmitedAlertsModel);
  _htmlLastEmitedAlertsView->setTableClass("table table-condensed table-hover");
  _htmlLastEmitedAlertsView->setMaxrows(50);
  _htmlLastEmitedAlertsView->setEmptyPlaceholder("(no alert)");
  _htmlLastEmitedAlertsView
      ->setEllipsePlaceholder("(alerts list too long to be displayed)");
  _htmlLastEmitedAlertsView10->setModel(_lastEmitedAlertsModel);
  _htmlLastEmitedAlertsView10
      ->setTableClass("table table-condensed table-hover");
  _htmlLastEmitedAlertsView10->setMaxrows(10);
  _htmlLastEmitedAlertsView10->setEmptyPlaceholder("(no alert)");
  _htmlLastEmitedAlertsView10
      ->setEllipsePlaceholder("(see alerts page for more alerts)");
  _htmlAlertRulesView->setModel(_alertRulesModel);
  _htmlAlertRulesView->setTableClass("table table-condensed table-hover");
  _htmlAlertRulesView->setHtmlPrefixRole(TextViews::HtmlPrefixRole);
  _htmlLogView->setModel(_memoryLogger->model());
  _htmlLogView->setTableClass("table table-condensed table-hover");
  _htmlLogView->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlLogView->setTrClassRole(LogModel::TrClassRole);
  _htmlLogView
      ->setEllipsePlaceholder("(download full raw log for more entries)");
  _htmlLogView->setEmptyPlaceholder("(empty log)");
  _htmlLogView10->setModel(_memoryLogger->model());
  _htmlLogView10->setTableClass("table table-condensed table-hover");
  _htmlLogView10->setHtmlPrefixRole(LogModel::HtmlPrefixRole);
  _htmlLogView10->setTrClassRole(LogModel::TrClassRole);
  _htmlLogView10->setMaxrows(10);
  _htmlLogView10->setEllipsePlaceholder("(see log page for more entries)");
  _htmlLogView10->setEmptyPlaceholder("(empty log)");
  _csvTasksTreeView->setModel(_tasksTreeModel);
  _csvTargetsTreeView->setModel(_targetsTreeModel);
  _csvHostsListView->setModel(_hostsListModel);
  _csvClustersListView->setModel(_clustersListModel);
  _csvResourceAllocationView->setModel(_resourceAllocationModel);
  _csvResourceAllocationView->setRowHeaders();
  _csvGlobalParamsView->setModel(_globalParamsModel);
  _csvAlertParamsView->setModel(_alertParamsModel);
  _csvRaisedAlertsView->setModel(_raisedAlertsModel);
  _csvLastEmitedAlertsView->setModel(_lastEmitedAlertsModel);
  _csvLogView->setModel(_memoryLogger->model());
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
  _wuiHandler->addView("log", _htmlLogView);
  _wuiHandler->addView("log10", _htmlLogView10);
  _memoryLogger->model()
      ->setWarningIcon("<i class=\"icon-warning-sign\"></i> ");
  _memoryLogger->model()->setErrorIcon("<i class=\"icon-minus-sign\"></i> ");
  _memoryLogger->model()->setWarningTrClass("warning");
  _memoryLogger->model()->setErrorTrClass("error");
}

QString WebConsole::name() const {
  return "WebConsole";
}

bool WebConsole::acceptRequest(const HttpRequest &req) {
  Q_UNUSED(req)
  return true;
}

void WebConsole::handleRequest(HttpRequest &req, HttpResponse &res) {
  QString path = req.url().path();
  while (path.size() && path.at(path.size()-1) == '/')
    path.chop(1);
  if (path.isEmpty()) {
    res.redirect("/console/");
    return;
  }
  if (path.startsWith("/console")) {
    /*res.setContentType("text/html;charset=UTF-8");
    res.output()->write("<html><head><title>Qron Web Console</title></head>\n"
                        "<body>\n");
    res.output()->write(_htmlTasksView->text().toUtf8().constData());*/
    _wuiHandler->handleRequest(req, res);
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
  // LATER tasks list (not only tree)
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
    res.output()->write(_htmlLogView->text().toUtf8().constData());
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
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
               _hostsListModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    disconnect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
               _clustersListModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    disconnect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QMap<QString,QMap<QString,qint64> >)),
               _resourceAllocationModel, SLOT(setResourceConfiguration(QMap<QString,QMap<QString,qint64> >)));
    disconnect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QMap<QString,qint64>)),
               _resourceAllocationModel, SLOT(setResourceAllocationForHost(QString,QMap<QString,qint64>)));
    disconnect(_scheduler, SIGNAL(taskStarted(TaskRequest,Host)),
               _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
    disconnect(_scheduler, SIGNAL(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)),
               _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
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
    disconnect(_scheduler->alerter(), SIGNAL(alertEmited(QString)),
               _lastEmitedAlertsModel, SLOT(alertEmited(QString)));
    disconnect(_scheduler->alerter(), SIGNAL(rulesChanged(QList<AlertRule>)),
               _alertRulesModel, SLOT(rulesChanged(QList<AlertRule>)));
  }
  _scheduler = scheduler;
  if (_scheduler) {
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
            _tasksTreeModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
            _targetsTreeModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
            _hostsListModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    connect(_scheduler, SIGNAL(targetsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
            _clustersListModel, SLOT(setAllHostsAndClusters(QMap<QString,Cluster>,QMap<QString,Host>)));
    connect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QMap<QString,QMap<QString,qint64> >)),
            _resourceAllocationModel, SLOT(setResourceConfiguration(QMap<QString,QMap<QString,qint64> >)));
    connect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QMap<QString,qint64>)),
            _resourceAllocationModel, SLOT(setResourceAllocationForHost(QString,QMap<QString,qint64>)));
    connect(_scheduler, SIGNAL(taskStarted(TaskRequest,Host)),
            _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
    connect(_scheduler, SIGNAL(taskFinished(TaskRequest,Host,bool,int,QWeakPointer<Executor>)),
            _tasksTreeModel, SLOT(taskChanged(TaskRequest)));
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
    connect(_scheduler->alerter(), SIGNAL(alertEmited(QString)),
            _lastEmitedAlertsModel, SLOT(alertEmited(QString)));
    connect(_scheduler->alerter(), SIGNAL(rulesChanged(QList<AlertRule>)),
            _alertRulesModel, SLOT(rulesChanged(QList<AlertRule>)));
    Log::addLogger(_memoryLogger, false);
  }
}
