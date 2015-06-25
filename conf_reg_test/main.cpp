#include <QCoreApplication>
#include <QtDebug>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFile>
#include <QBuffer>
#include <QDir>
#include "config/schedulerconfig.h"
#include "configmgt/localconfigrepository.h"
#include "log/log.h"

#define TESTS_DIR "./test_configs/."

int main(int argc, char *argv[]) {
  QCoreApplication a(argc, argv);
  LocalConfigRepository repo(0, 0);
  bool success = true;
  QDir testsDir(TESTS_DIR);
  QFileInfoList fileInfos =
      testsDir.entryInfoList(QDir::Files, QDir::Name);
  if (argc > 1 && strstr(argv[1], "-v") == argv[1]) {
    Log::addConsoleLogger(Log::Info);
  }
  foreach (const QFileInfo &fi, fileInfos) {
    QFile f(fi.filePath());
    if (f.open(QIODevice::ReadOnly)) {
      SchedulerConfig config = repo.parseConfig(&f, false);
      if (!config.isNull()) {
        QByteArray array;
        QBuffer buf(&array);
        buf.open(QIODevice::ReadWrite);
        qint64 size = config.writeAsPf(&buf);
        buf.seek(0);
        SchedulerConfig config2 = repo.parseConfig(&buf, false);
        QFile f1(fi.baseName()+".iteration1");
        if (!f1.open(QIODevice::WriteOnly)
            || config.writeAsPf(&f1) < 0) {
          qDebug() << "cannot write file:" << f1.fileName() << ":"
                   << f1.errorString();
          success = false;
        }
        QFile f2(fi.baseName()+".iteration2");
        if (!f2.open(QIODevice::WriteOnly)
            || config2.writeAsPf(&f2) < 0) {
          qDebug() << "cannot write file:" << f2.fileName() << ":"
                   << f2.errorString();
          success = false;
        }
        if (size > 0 && config.id() == config2.id()) {
          qDebug() << "ok" << fi.baseName() << config.id() << buf.size();
        } else {
          qDebug() << "KO" << fi.baseName()
                   << ": differs from output : original :" << config.id()
                   << "dumped :" << config2.id() << buf.size()
                   << ". diff -NubBw " << f1.fileName()
                   << f2.fileName();
          success = false;
        }
      } else {
        qDebug() << "warn" << fi.baseName() << ": invalid :" << f.errorString();
      }
    } else {
      qDebug() << "KO" << fi.baseName() << ": cannot open :" << f.errorString();
      success = false;
    }
  }
  return success ? 0 : 1;
}
