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
#include <QCoreApplication>
#include "sched/scheduler.h"
#include <QFile>
#include <QtDebug>
#include "log/log.h"
#include "log/filelogger.h"
#include "httpd/httpserver.h"
#include "ui/webconsole.h"
#include <QThread>
#include <unistd.h>

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);
  QThread::currentThread()->setObjectName("MainThread");
  Log::addConsoleLogger();
  Scheduler *scheduler = new Scheduler;
  HttpServer *httpd = new HttpServer;
  WebConsole *webconsole = new WebConsole;
  webconsole->setScheduler(scheduler);
  httpd->appendHandler(webconsole);
  httpd->listen(QHostAddress::Any, 8086); // TODO parametrize addr and port
  QFile *config = new QFile("./qron.conf"); // TODO parametrize config file
  // LATER have a config directory rather only one file
  QString errorString;
  int rc = scheduler->loadConfiguration(config, errorString) ? 0 : 1;
  delete config;
  if (rc) {
    Log::fatal() << "cannot load configuration: " << errorString;
    Log::fatal() << "qrond is aborting startup sequence";
  } else {
    // LATER daemonize on Unix
    // LATER catch Unix signals
    // EVENLATER servicize on Windows
    rc = a.exec();
  }
  delete httpd;
  delete scheduler;
  ::usleep(100000); // give a chance for last asynchronous log writing
  Log::clearLoggers(); // this deletes logger and console
  return rc;
}
