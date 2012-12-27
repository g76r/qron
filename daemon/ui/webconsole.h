#ifndef WEBCONSOLE_H
#define WEBCONSOLE_H

#include "httpd/httphandler.h"
#include "taskstreemodel.h"
#include "textview/htmltableview.h"
#include "textview/csvview.h"
#include "sched/scheduler.h"
#include <QObject>
#include "httpd/templatinghttphandler.h"

class WebConsole : public HttpHandler {
  Q_OBJECT
  Scheduler *_scheduler;
  TasksTreeModel *_tasksModel;
  HtmlTableView *_htmlTasksView;
  CsvView *_csvTasksView;
  TemplatingHttpHandler *_wuiHandler;

public:
  WebConsole(QObject *parent = 0);
  QString name() const;
  bool acceptRequest(const HttpRequest &req);
  void handleRequest(HttpRequest &req, HttpResponse &res);
  void setScheduler(Scheduler *scheduler);
};

#endif // WEBCONSOLE_H
