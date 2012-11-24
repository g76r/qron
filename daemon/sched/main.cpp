#include <QCoreApplication>
#include "sched/scheduler.h"
#include <QFile>
#include <QtDebug>
#include "log/log.h"
#include "log/filelogger.h"

int main(int argc, char *argv[]) {
  //qInstallMsgHandler(Log::logMessageHandler);
  QCoreApplication a(argc, argv);
  QFile *console = new QFile;
  console->open(1, QIODevice::WriteOnly|QIODevice::Unbuffered);
  FileLogger *logger = new FileLogger(console, Log::Debug, 0);
  Log::addLogger(logger);
  Scheduler *scheduler = new Scheduler;
  QFile *config = new QFile("./qron.conf");
  QString errorString;
  if (!scheduler->loadConfiguration(config, errorString)) {
    Log::fatal() << "cannot load configuration: " << errorString;
    Log::fatal() << "qrond is aborting startup sequence";
    delete console;
    delete logger;
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
                   QCoreApplication::instance(), SLOT(quit()));
  return a.exec();
}
