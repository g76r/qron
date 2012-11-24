#include "timerwithargument.h"
#include <QMetaObject>
#include "log/log.h"

TimerWithArgument::TimerWithArgument(QObject *parent) : QTimer(parent) {
  connect(this, SIGNAL(timeout()), this, SLOT(forwardTimeout()));
}

void TimerWithArgument::connectWithArgs(
    QObject *object, const char *member, Qt::ConnectionType connectionType,
    QVariant arg0, QVariant arg1, QVariant arg2, QVariant arg3, QVariant arg4,
    QVariant arg5, QVariant arg6, QVariant arg7, QVariant arg8, QVariant arg9) {
  if (object && member) {
    _object = object;
    const char *parenthesis;
    if (*member >= '0' && *member <= '9' && (parenthesis = strchr(member, '(')))
      _member = QString::fromUtf8(member+1, parenthesis-member-1);
    else
      _member = member;
    //qDebug() << "    member" << _member << member;
    _connectionType = connectionType;
    _arg[0] = arg0;
    _arg[1] = arg1;
    _arg[2] = arg2;
    _arg[3] = arg3;
    _arg[4] = arg4;
    _arg[5] = arg5;
    _arg[6] = arg6;
    _arg[7] = arg7;
    _arg[8] = arg8;
    _arg[9] = arg9;
  }
}

void TimerWithArgument::forwardTimeout() {
  if (_object && !_member.isEmpty()) {

    if (!QMetaObject::invokeMethod(
          _object.data(), _member.toUtf8().constData(), _connectionType,
          _arg[0].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[0]),
          _arg[1].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[1]),
          _arg[2].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[2]),
          _arg[3].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[3]),
          _arg[4].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[4]),
          _arg[5].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[5]),
          _arg[6].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[6]),
          _arg[7].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[7]),
          _arg[8].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[8]),
          _arg[9].isNull() ? QGenericArgument() : Q_ARG(QVariant, _arg[9]))) {
      Log::warning() << "cannot signal timer timeout to " << _object.data()
                     << _member << _connectionType << _arg[0] << _arg[1];
    }
  } else {
    Log::warning() << "timer timeout occur before target is configured";
  }
}

void TimerWithArgument::singleShot(
    int msec, QObject *receiver, const char *member,
    Qt::ConnectionType connectionType, QVariant arg0, QVariant arg1,
    QVariant arg2, QVariant arg3, QVariant arg4, QVariant arg5, QVariant arg6,
    QVariant arg7, QVariant arg8, QVariant arg9) {
  TimerWithArgument *t = new TimerWithArgument;
  t->setSingleShot(true);
  // this is less optimized than the internal mechanism of QSingleShotTimer
  connect(t, SIGNAL(timeout()), t, SLOT(deleteLater()));
  t->connectWithArgs(receiver, member, connectionType, arg0, arg1, arg2, arg3,
                     arg4, arg5, arg6, arg7, arg8, arg9);
  t->start(msec);
}
