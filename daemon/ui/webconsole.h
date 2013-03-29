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
#ifndef WEBCONSOLE_H
#define WEBCONSOLE_H

#include "httpd/httphandler.h"
#include "httpd/templatinghttphandler.h"
#include "textview/htmltableview.h"
#include "textview/csvtableview.h"
#include "textview/htmltreeview.h"
#include "textview/csvtreeview.h"
#include "sched/scheduler.h"
#include "taskstreemodel.h"
#include "targetstreemodel.h"
#include "resourcesallocationmodel.h"
#include "hostslistmodel.h"
#include "clusterslistmodel.h"
#include "util/paramsetmodel.h"
#include "raisedalertsmodel.h"
#include "lastoccuredtexteventsmodel.h"
#include "textview/clockview.h"
#include "alertrulesmodel.h"
#include "log/memorylogger.h"
#include "taskrequestsmodel.h"
#include "tasksmodel.h"
#include "schedulereventsmodel.h"
#include "flagssetmodel.h"
#include "taskgroupsmodel.h"
#include "lastemitedalertsmodel.h"

class QThread;

class WebConsole : public HttpHandler {
  Q_OBJECT
  QThread *_thread;
  Scheduler *_scheduler;
  TasksTreeModel *_tasksTreeModel;
  TargetsTreeModel *_targetsTreeModel;
  HostsListModel *_hostsListModel;
  ClustersListModel *_clustersListModel;
  ResourcesAllocationModel *_resourceAllocationModel;
  ParamSetModel *_globalParamsModel, *_alertParamsModel;
  RaisedAlertsModel *_raisedAlertsModel;
  LastEmitedAlertsModel *_lastEmitedAlertsModel;
  LastOccuredTextEventsModel *_lastPostedNoticesModel, *_lastFlagsChangesModel;
  AlertRulesModel *_alertRulesModel;
  TaskRequestsModel *_taskRequestsHistoryModel, *_unfinishedTaskRequestModel;
  TasksModel *_tasksModel;
  SchedulerEventsModel *_schedulerEventsModel;
  FlagsSetModel *_flagsSetModel;
  TaskGroupsModel *_taskGroupsModel;
  HtmlTreeView *_htmlTasksTreeView, *_htmlTargetsTreeView;
  HtmlTableView *_htmlHostsListView, *_htmlClustersListView,
  *_htmlResourcesAllocationView, *_htmlGlobalParamsView, *_htmlAlertParamsView,
  *_htmlRaisedAlertsView, *_htmlRaisedAlertsView10, *_htmlLastEmitedAlertsView,
  *_htmlLastEmitedAlertsView10, *_htmlAlertRulesView, *_htmlWarningLogView,
  *_htmlWarningLogView10, *_htmlInfoLogView,
  *_htmlTaskRequestsView, *_htmlTaskRequestsView20,
  *_htmlTasksScheduleView, *_htmlTasksConfigView, *_htmlTasksParamsView,
  *_htmlTasksListView,
  *_htmlTasksEventsView, *_htmlSchedulerEventsView,
  *_htmlLastPostedNoticesView20, *_htmlLastFlagsChangesView20,
  *_htmlFlagsSetView20, *_htmlTaskGroupsView, *_htmlTaskGroupsEventsView;
  ClockView *_clockView;
  CsvTableView *_csvTasksTreeView, *_csvTargetsTreeView, *_csvHostsListView,
  *_csvClustersListView, *_csvResourceAllocationView, *_csvGlobalParamsView,
  *_csvAlertParamsView, *_csvRaisedAlertsView, *_csvLastEmitedAlertsView,
  *_csvAlertRulesView, *_csvLogView, *_csvTaskRequestsView, *_csvTasksView,
  *_csvSchedulerEventsView, *_csvLastPostedNoticesView,
  *_csvLastFlagsChangesView, *_csvFlagsSetView, *_csvTaskGroupsView;
  TemplatingHttpHandler *_wuiHandler;
  MemoryLogger *_memoryInfoLogger, *_memoryWarningLogger;
  QString _title, _navtitle;

public:
  WebConsole();
  QString name() const;
  bool acceptRequest(HttpRequest req);
  void handleRequest(HttpRequest req, HttpResponse res);
  void setScheduler(Scheduler *scheduler);

signals:
  void flagChange(QString change, int type);
  void alertEmited(QString alert, int type);

private slots:
  void flagSet(QString flag);
  void flagCleared(QString flag);
  void alertEmited(QString alert);
  void alertCancellationEmited(QString alert);
  void globalParamsChanged(ParamSet globalParams);
};

#endif // WEBCONSOLE_H
