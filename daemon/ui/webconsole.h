#ifndef WEBCONSOLE_H
#define WEBCONSOLE_H

#include "httpd/httphandler.h"
#include "taskstreemodel.h"
#include "textview/textview.h"
#include "sched/scheduler.h"
#include <QObject>

class WebConsole : public HttpHandler {
  Q_OBJECT
  Scheduler *_scheduler;
  TasksTreeModel *_tasksModel;
  TextView *_htmlTasksView, *_csvTasksView;

public:
  WebConsole(QObject *parent = 0);
  QString name() const;
  bool acceptRequest(const HttpRequest &req);
  void handleRequest(HttpRequest &req, HttpResponse &res);
  void setScheduler(Scheduler *scheduler);
};

#endif // WEBCONSOLE_H
