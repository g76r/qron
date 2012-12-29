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
#ifndef TIMERWITHARGUMENT_H
#define TIMERWITHARGUMENT_H

#include <QTimer>
#include <QVariant>
#include <QWeakPointer>

class TimerWithArgument : public QTimer {
  Q_OBJECT
  QWeakPointer<QObject> _object;
  QString _member;
  Qt::ConnectionType _connectionType;
  QVariant _arg[10];

public:
  explicit TimerWithArgument(QObject *parent = 0);
  /**
    * @param member is either the raw method name (e.g. "foo") either a SLOT
    *        macro (e.g. SLOT(foo(QVariant,QVariant)))
    */
  void connectWithArgs(QObject *object, const char *member,
                       Qt::ConnectionType connectionType,
                       QVariant arg0 = QVariant(), QVariant arg1 = QVariant(),
                       QVariant arg2 = QVariant(), QVariant arg3 = QVariant(),
                       QVariant arg4 = QVariant(), QVariant arg5 = QVariant(),
                       QVariant arg6 = QVariant(), QVariant arg7 = QVariant(),
                       QVariant arg8 = QVariant(), QVariant arg9 = QVariant());
  void connectWithArgs(QObject *object, const char *member,
                       QVariant arg0 = QVariant(), QVariant arg1 = QVariant(),
                       QVariant arg2 = QVariant(), QVariant arg3 = QVariant(),
                       QVariant arg4 = QVariant(), QVariant arg5 = QVariant(),
                       QVariant arg6 = QVariant(), QVariant arg7 = QVariant(),
                       QVariant arg8 = QVariant(), QVariant arg9 = QVariant()) {
    connectWithArgs(object, member, Qt::AutoConnection, arg0, arg1, arg2, arg3,
                    arg4, arg5, arg6, arg7, arg8, arg9);
  }
  static void singleShot(int msec, QObject *receiver, const char *member,
                         Qt::ConnectionType connectionType,
                         QVariant arg0 = QVariant(), QVariant arg1 = QVariant(),
                         QVariant arg2 = QVariant(), QVariant arg3 = QVariant(),
                         QVariant arg4 = QVariant(), QVariant arg5 = QVariant(),
                         QVariant arg6 = QVariant(), QVariant arg7 = QVariant(),
                         QVariant arg8 = QVariant(),
                         QVariant arg9 = QVariant());
  static void singleShot(int msec, QObject *receiver, const char *member,
                         QVariant arg0 = QVariant(), QVariant arg1 = QVariant(),
                         QVariant arg2 = QVariant(), QVariant arg3 = QVariant(),
                         QVariant arg4 = QVariant(), QVariant arg5 = QVariant(),
                         QVariant arg6 = QVariant(), QVariant arg7 = QVariant(),
                         QVariant arg8 = QVariant(),
                         QVariant arg9 = QVariant()) {
    singleShot(msec, receiver, member, Qt::AutoConnection, arg0, arg1, arg2,
               arg3, arg4, arg5, arg6, arg7, arg8, arg9);
  }

private slots:
  void forwardTimeout();
};

#endif // TIMERWITHARGUMENT_H
