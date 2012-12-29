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
#ifndef WEBCONSOLE_H
#define WEBCONSOLE_H

#include "httpd/httphandler.h"
#include "httpd/templatinghttphandler.h"
#include "textview/htmltableview.h"
#include "textview/csvview.h"
#include "sched/scheduler.h"
#include "taskstreemodel.h"
#include "targetstreemodel.h"

class WebConsole : public HttpHandler {
  Q_OBJECT
  Scheduler *_scheduler;
  TasksTreeModel *_tasksModel;
  TargetsTreeModel *_hostsModel;
  HtmlTableView *_htmlTasksView, *_htmlHostsView;
  CsvView *_csvTasksView, *_csvHostsView;
  TemplatingHttpHandler *_wuiHandler;

public:
  WebConsole(QObject *parent = 0);
  QString name() const;
  bool acceptRequest(const HttpRequest &req);
  void handleRequest(HttpRequest &req, HttpResponse &res);
  void setScheduler(Scheduler *scheduler);
};

#endif // WEBCONSOLE_H
