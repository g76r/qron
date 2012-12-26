#include "webconsole.h"

WebConsole::WebConsole(QObject *parent) : HttpHandler(parent), _scheduler(0),
  _tasksModel(new TasksTreeModel(this)), _tasksView(new TreeHtmlView(this)) {
  _tasksView->setModel(_tasksModel);
}

QString WebConsole::name() const {
  return "WebConsole";
}

bool WebConsole::acceptRequest(const HttpRequest &req) {
  const QString path = req.url().path();
  return path.isEmpty() || path == "/" || path.startsWith("/console");
}

void WebConsole::handleRequest(HttpRequest &req, HttpResponse &res) {
  const QString path = req.url().path();
  if (path.isEmpty() || path == "/") {
    // FIXME redirect
  } else {
    // FIXME
  }
  res.setContentType("text/html");
  res.output()->write("<html><head><title>Qron Web Console</title></head>\n"
                      "<body>\n");
  res.output()->write(_tasksView->text().toUtf8().constData());
}

void WebConsole::setScheduler(Scheduler *scheduler) {
  if (_scheduler) {
    disconnect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
               _tasksModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
  }
  _scheduler = scheduler;
  if (_scheduler) {
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
            _tasksModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
  }
}
