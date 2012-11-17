#include <QCoreApplication>
#include "sched/scheduler.h"
#include <QFile>
#include <QtDebug>

int main(int argc, char *argv[]) {
  // TODO redirect qDebug() to log files
  QCoreApplication a(argc, argv);
  Scheduler scheduler;
  QFile *file = new QFile("./qron.conf");
  if (!scheduler.loadConfiguration(file)) {
    qCritical() << "cannot load configuration, aborting";
    return 1;
  }
  file->deleteLater();
  return a.exec();
}
