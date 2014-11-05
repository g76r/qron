#include "configrepository.h"
#include "pf/pfdomhandler.h"
#include <QThread>

ConfigRepository::ConfigRepository(QObject *parent, Scheduler *scheduler)
  : QObject(parent), _scheduler(scheduler) {
}

SchedulerConfig ConfigRepository::parseConfig(QIODevice *source) {
  if (!source->isOpen())
    if (!source->open(QIODevice::ReadOnly)) {
      QString errorString = source->errorString();
      Log::error() << "cannot read configuration: " << errorString;
      return SchedulerConfig();
    }
  PfDomHandler pdh;
  PfParser pp(&pdh);
  pp.parse(source);
  if (pdh.errorOccured()) {
    QString errorString = pdh.errorString()+" at line "
        +QString::number(pdh.errorLine())
        +" column "+QString::number(pdh.errorColumn());
    Log::error() << "empty or invalid configuration: " << errorString;
    return SchedulerConfig();
  }
  QList<PfNode> roots = pdh.roots();
  if (roots.size() == 0) {
    Log::error() << "configuration lacking root node";
  } else if (roots.size() == 1) {
    PfNode &root(roots.first());
    if (root.name() == "qrontab") {
      SchedulerConfig config;
      if (QThread::currentThread() == thread())
        config = parseConfig(root);
      else
        QMetaObject::invokeMethod(this, "parseConfig",
                                  Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(SchedulerConfig, config),
                                  Q_ARG(PfNode, root));
      return config;
    } else {
      Log::error() << "configuration root node is not \"qrontab\"";
    }
  } else {
    Log::error() << "configuration with more than one root node";
  }
  return SchedulerConfig();
}

SchedulerConfig ConfigRepository::parseConfig(PfNode root) {
  return SchedulerConfig(root, _scheduler, true);
}
