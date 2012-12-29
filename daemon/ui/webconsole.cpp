/* Copyright 2012 Hallowyn and others.
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

WebConsole::WebConsole(QObject *parent) : HttpHandler(parent), _scheduler(0),
  _tasksTreeModel(new TasksTreeModel(this)),
  _targetsTreeModel(new TargetsTreeModel(this)),
  _resourceAllocationModel(new ResourceAllocationModel(this)),
  _htmlTasksTreeView(new HtmlTableView(this)),
  _htmlTargetsTreeView(new HtmlTableView(this)),
  _htmlResourceAllocationView(new HtmlTableView(this)),
  _csvTasksTreeView(new CsvView(this)),
  _csvTargetsTreeView(new CsvView(this)),
  _csvResourceAllocationView(new CsvView(this)),
  _wuiHandler(new TemplatingHttpHandler(this, "/console",
                                        ":docroot/console")) {
  _htmlTasksTreeView->setModel(_tasksTreeModel);
  _htmlTasksTreeView->setTableClass("table table-condensed table-hover");
  _htmlTasksTreeView->setTrClassRole(TasksTreeModel::TrClassRole);
  _htmlTasksTreeView->setLinkRole(TasksTreeModel::LinkRole);
  _htmlTasksTreeView->setHtmlPrefixRole(TasksTreeModel::HtmlPrefixRole);
  _htmlTargetsTreeView->setModel(_targetsTreeModel);
  _htmlTargetsTreeView->setTableClass("table table-condensed table-hover");
  _htmlTargetsTreeView->setTrClassRole(TargetsTreeModel::TrClassRole);
  _htmlTargetsTreeView->setLinkRole(TargetsTreeModel::LinkRole);
  _htmlTargetsTreeView->setHtmlPrefixRole(TargetsTreeModel::HtmlPrefixRole);
  _htmlResourceAllocationView->setModel(_resourceAllocationModel);
  _htmlResourceAllocationView->setTableClass("table table-condensed "
                                             "table-hover table-bordered");
  _htmlResourceAllocationView->setRowHeaders();
  _htmlResourceAllocationView->setTopLeftHeader(QString::fromUtf8("Host × Resource"));
  _htmlResourceAllocationView
      ->setHtmlPrefixRole(ResourceAllocationModel::HtmlPrefixRole);
  _csvTasksTreeView->setModel(_tasksTreeModel);
  _csvTargetsTreeView->setModel(_targetsTreeModel);
  _csvResourceAllocationView->setModel(_resourceAllocationModel);
  _wuiHandler->addFilter("\\.html$");
  _wuiHandler->addView("taskstree", _htmlTasksTreeView);
  _wuiHandler->addView("targetstree", _htmlTargetsTreeView);
  _wuiHandler->addView("resourceallocation", _htmlResourceAllocationView);
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
  if (path == "/rest/csv/tasks/tree/v1") {
    res.setContentType("text/csv;charset=UTF-8");
    res.output()->write(_csvTasksTreeView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/tasks/tree/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTasksTreeView->text().toUtf8().constData());
    return;
  }
  res.setStatus(404);
  res.output()->write("Not found.");
}

void WebConsole::setScheduler(Scheduler *scheduler) {
  if (_scheduler) {
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
               _tasksTreeModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    disconnect(_scheduler, SIGNAL(hostsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
               _targetsTreeModel, SLOT(setAllHostsAndGroups(QMap<QString,Cluster>,QMap<QString,Host>)));
    disconnect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QMap<QString,QMap<QString,qint64> >)),
               _resourceAllocationModel, SLOT(setResourceConfiguration(QMap<QString,QMap<QString,qint64> >)));
    disconnect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QMap<QString,qint64>)),
               _resourceAllocationModel, SLOT(setResourceAllocationForHost(QString,QMap<QString,qint64>)));
  }
  _scheduler = scheduler;
  if (_scheduler) {
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
            _tasksTreeModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    connect(_scheduler, SIGNAL(hostsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
            _targetsTreeModel, SLOT(setAllHostsAndGroups(QMap<QString,Cluster>,QMap<QString,Host>)));
    connect(_scheduler, SIGNAL(hostResourceConfigurationChanged(QMap<QString,QMap<QString,qint64> >)),
            _resourceAllocationModel, SLOT(setResourceConfiguration(QMap<QString,QMap<QString,qint64> >)));
    connect(_scheduler, SIGNAL(hostResourceAllocationChanged(QString,QMap<QString,qint64>)),
            _resourceAllocationModel, SLOT(setResourceAllocationForHost(QString,QMap<QString,qint64>)));
  }
}
