#include "webconsole.h"

WebConsole::WebConsole(QObject *parent) : HttpHandler(parent), _scheduler(0),
  _tasksModel(new TasksTreeModel(this)),
  _htmlTasksView(new HtmlTableView(this)), _csvTasksView(new CsvView(this)),
  _wuiHandler(new TemplatingHttpHandler(this, "/console",
                                        ":docroot/console")) {
  _htmlTasksView->setModel(_tasksModel);
  _htmlTasksView->setTableClass("table table-condensed table-hover");
  _htmlTasksView->setTrClassRole(TasksTreeModel::TrClassRole);
  _htmlTasksView->setLinkRole(TasksTreeModel::LinkRole);
  _htmlTasksView->setHtmlPrefixRole(TasksTreeModel::HtmlPrefixRole);
  _csvTasksView->setModel(_tasksModel);
  _wuiHandler->addFilter("\\.html$");
  _wuiHandler->addView("tasksConfig", _htmlTasksView);
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
  }
  _scheduler = scheduler;
  if (_scheduler) {
    connect(_scheduler, SIGNAL(tasksConfigurationReset(QMap<QString,TaskGroup>,QMap<QString,Task>)),
            _tasksModel, SLOT(setAllTasksAndGroups(QMap<QString,TaskGroup>,QMap<QString,Task>)));
  }
}
