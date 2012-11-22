#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <QObject>
#include "taskrequest.h"
#include "data/host.h"
#include <QWeakPointer>
#include <QProcess>

class QThread;

class Executor : public QObject {
  Q_OBJECT
  bool _isTemporary;
  QThread *_thread;
  QProcess *_process;
  QByteArray _errBuf;
  TaskRequest _request;
  Host _target;

public:
  /** @param threadParent will be used as QThread parent
    */
  explicit Executor(QObject *threadParent);
  void setTemporary(bool temporary = true) { _isTemporary = temporary; }
  bool isTemporary() const { return _isTemporary; }
  
public slots:
  /** Execute a request now.
    * This method is thread-safe.
    */
  void execute(TaskRequest request, Host target);

signals:
  void taskFinished(TaskRequest request, Host target, bool success,
                    int returnCode, QWeakPointer<Executor> executor);

private slots:
  void error(QProcess::ProcessError error);
  void finished(int exitCode, QProcess::ExitStatus exitStatus);
  void readyReadStandardError();
  void readyReadStandardOutput();

private:
  Q_INVOKABLE void doExecute(TaskRequest request, Host target);
  void execMean(TaskRequest request, Host target);
  void sshMean(TaskRequest request, Host target);
  void httpMean(TaskRequest request, Host target);
  void execProcess(TaskRequest request, Host target, QStringList cmdline);
  Q_DISABLE_COPY(Executor)
};

#endif // EXECUTOR_H
