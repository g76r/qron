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
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "webconsole.h"

WebConsole::WebConsole(QObject *parent) : HttpHandler(parent), _scheduler(0),
  _tasksModel(new TasksTreeModel(this)),
  _hostsModel(new TargetsTreeModel(this)),
  _htmlTasksView(new HtmlTableView(this)),
  _htmlHostsView(new HtmlTableView(this)),
  _csvTasksView(new CsvView(this)),
  _csvHostsView(new CsvView(this)),
  _wuiHandler(new TemplatingHttpHandler(this, "/console",
                                        ":docroot/console")) {
  _htmlTasksView->setModel(_tasksModel);
  _htmlTasksView->setTableClass("table table-condensed table-hover");
  _htmlTasksView->setTrClassRole(TasksTreeModel::TrClassRole);
  _htmlTasksView->setLinkRole(TasksTreeModel::LinkRole);
  _htmlTasksView->setHtmlPrefixRole(TasksTreeModel::HtmlPrefixRole);
  _htmlHostsView->setModel(_hostsModel);
  _htmlHostsView->setTableClass("table table-condensed table-hover");
  _htmlHostsView->setTrClassRole(TargetsTreeModel::TrClassRole);
  _htmlHostsView->setLinkRole(TargetsTreeModel::LinkRole);
  _htmlHostsView->setHtmlPrefixRole(TargetsTreeModel::HtmlPrefixRole);
  _csvTasksView->setModel(_tasksModel);
  _csvHostsView->setModel(_hostsModel);
  _wuiHandler->addFilter("\\.html$");
  _wuiHandler->addView("taskstree", _htmlTasksView);
  _wuiHandler->addView("targetstree", _htmlHostsView);
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
    res.output()->write(_csvTasksView->text().toUtf8().constData());
    return;
  }
  if (path == "/rest/html/tasks/tree/v1") {
    res.setContentType("text/html;charset=UTF-8");
    res.output()->write(_htmlTasksView->text().toUtf8().constData());
    return;
  }
  res.setStatus(404);
  res.output()->write("Not found.");
}

void WebConsole::setScheduler(Scheduler *scheduler) {
  if (_scheduler) {
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
               _tasksModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    disconnect(_scheduler, SIGNAL(hostsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
               _hostsModel, SLOT(setAllHostsAndGroups(QMap<QString,Cluster>,QMap<QString,Host>)));
  }
  _scheduler = scheduler;
  if (_scheduler) {
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
            _tasksModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
    connect(_scheduler, SIGNAL(hostsConfigurationReset(QMap<QString,Cluster>,QMap<QString,Host>)),
            _hostsModel, SLOT(setAllHostsAndGroups(QMap<QString,Cluster>,QMap<QString,Host>)));
  }
}
