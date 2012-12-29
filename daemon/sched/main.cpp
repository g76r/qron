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

int main(int argc, char *argv[]) {
  //qInstallMsgHandler(Log::logMessageHandler);
  QCoreApplication a(argc, argv);
  QFile *console = new QFile;
  console->open(1, QIODevice::WriteOnly|QIODevice::Unbuffered);
  FileLogger *logger = new FileLogger(console, Log::Debug, 0);
  Log::addLogger(logger);
  Scheduler *scheduler = new Scheduler;
  HttpServer *httpd = new HttpServer;
  WebConsole *webconsole = new WebConsole;
  httpd->appendHandler(webconsole);
  webconsole->setScheduler(scheduler);
  httpd->listen(QHostAddress::Any, 8086); // TODO parametrize addr and port
  QFile *config = new QFile("./qron.conf");
  QString errorString;
  if (!scheduler->loadConfiguration(config, errorString)) {
    Log::fatal() << "cannot load configuration: " << errorString;
    Log::fatal() << "qrond is aborting startup sequence";
    delete console;
    delete webconsole;
    delete httpd;
    delete scheduler;
    delete config;
    return 1;
  }
  config->deleteLater();
  QObject::connect(scheduler, SIGNAL(destroyed()),
                   console, SLOT(deleteLater()));
  QObject::connect(scheduler, SIGNAL(destroyed()),
                   logger, SLOT(deleteLater()));
  QObject::connect(scheduler, SIGNAL(destroyed()),
                   httpd, SLOT(deleteLater()));
  QObject::connect(scheduler, SIGNAL(destroyed()),
                   QCoreApplication::instance(), SLOT(quit()));
  return a.exec();
}
