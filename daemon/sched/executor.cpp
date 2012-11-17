#include "executor.h"
#include <QThread>
#include <QtDebug>

Executor::Executor(QObject *parent) : QObject(parent), _isTemporary(false),
  _thread(new QThread(parent)) {
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
}

void Executor::execute(TaskRequest request, Host target) {
  // TODO ssh and http means
  // LATER https, postevent, setflag, clearflag, exec means
  qDebug() << "TODO execute task" << request.task();
  emit taskFinished(request, target, false, 42, this);
}
