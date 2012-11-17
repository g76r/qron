#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <QObject>
#include "taskrequest.h"
#include "data/host.h"
#include <QWeakPointer>

class QThread;

class Executor : public QObject {
  Q_OBJECT
  bool _isTemporary;
  QThread *_thread;

public:
  /** @param threadParent will be used as QThread parent
    */
  explicit Executor(QObject *threadParent);
  void setTemporary(bool temporary = true) { _isTemporary = temporary; }
  bool isTemporary() const { return _isTemporary; }
  
public slots:
  void execute(TaskRequest request, Host target);

signals:
  void taskFinished(TaskRequest request, Host target, bool success,
                    int returnCode, QWeakPointer<Executor> executor);

private:
  Q_DISABLE_COPY(Executor)
};

#endif // EXECUTOR_H
