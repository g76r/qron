#include "executor.h"
#include <QThread>
#include <QtDebug>
#include <QMetaObject>

Executor::Executor(QObject *threadParent) : QObject(0), _isTemporary(false),
  _thread(new QThread(threadParent)), _process(0) {
  connect(this, SIGNAL(destroyed(QObject*)), _thread, SLOT(quit()));
  connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater()));
  _thread->start();
  moveToThread(_thread);
  //qDebug() << "creating new task executor" << this;
}

void Executor::execute(TaskRequest request, Host target) {
  QMetaObject::invokeMethod(this, "doExecute", Q_ARG(TaskRequest, request),
                            Q_ARG(Host, target));
}

void Executor::doExecute(TaskRequest request, Host target) {
  const QString mean = request.task().mean();
  qDebug() << "executing task" << request.task() << "with mean" << mean;
  if (mean == "exec")
    execMean(request, target);
  else if (mean == "ssh")
    sshMean(request, target);
  else if (mean == "http")
    httpMean(request, target);
  else {
    qDebug() << "cannot execute task with unknown mean" << mean << ":"
             << request.task();
    emit taskFinished(request, target, false, 1, this);
  }
  // LATER add other means: https, postevent, setflag, clearflag
}

void Executor::execMean(TaskRequest request, Host target) {
  _request = request;
  _target = target;
  _errBuf.clear();
  _process = new QProcess(this);
  _process->setProcessChannelMode(QProcess::SeparateChannels);
  connect(_process, SIGNAL(error(QProcess::ProcessError)),
          this, SLOT(error(QProcess::ProcessError)));
  connect(_process, SIGNAL(finished(int,QProcess::ExitStatus)),
          this, SLOT(finished(int,QProcess::ExitStatus)));
  connect(_process, SIGNAL(readyReadStandardError()),
          this, SLOT(readyReadStandardError()));
  connect(_process, SIGNAL(readyReadStandardOutput()),
          this, SLOT(readyReadStandardOutput()));
   QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
   foreach (const QString key, request.params().keys()) {
     //qDebug() << "setting environment variable" << key << "="
     //         << request.params().value(key);
     env.insert(key, request.params().value(key));
   }
   _process->setProcessEnvironment(env);
   QStringList args =
       _request.params().splitAndEvaluate(_request.task().command());
   if (!args.isEmpty()) {
     QString program = args.takeFirst();
     qDebug() << "about to start" << program << "with args" << args
              << "using environment" << env.toStringList();
     _process->start(program, args);
   } else
     qWarning() << "cannot execute task with empty command"
                << request.task().fqtn();
}

void Executor::sshMean(TaskRequest request, Host target) {
  // TODO
  emit taskFinished(request, target, false, 42, this);
}

void Executor::error(QProcess::ProcessError error) {
  qDebug() << "task error" << error << _process->errorString();
}

void Executor::finished(int exitCode, QProcess::ExitStatus exitStatus) {
  readyReadStandardError();
  readyReadStandardOutput();
  //qDebug() << "task finished";
  emit taskFinished(_request, _target, exitStatus == QProcess::NormalExit
                    && exitCode == 0, exitCode, this);
  _process->deleteLater();
  _process = 0;
  _errBuf.clear();
  _target = Host();
  _request = TaskRequest();
}

void Executor::readyReadStandardError() {
  _process->setReadChannel(QProcess::StandardError);
  QByteArray ba;
  while (!(ba = _process->read(1024)).isEmpty()) {
    _errBuf.append(ba);
    int i;
    while (((i = _errBuf.indexOf('\n')) >= 0)) {
      QString line = QString::fromUtf8(_errBuf.mid(0, i));
      _errBuf.remove(0, i+1);
      // TODO clarify logs
      qDebug() << "task error output:" << line.trimmed();
    }
  }
}

void Executor::readyReadStandardOutput() {
  _process->setReadChannel(QProcess::StandardOutput);
  while (!_process->read(1024).isEmpty());
  // LATER make it possible to log stdout too
}

void Executor::httpMean(TaskRequest request, Host target) {
  // TODO
  emit taskFinished(request, target, false, 42, this);
}
