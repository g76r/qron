/* Copyright 2012-2013 Hallowyn and others.
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
#ifndef TASKREQUEST_H
#define TASKREQUEST_H

#include <QSharedDataPointer>
#include "task.h"
#include "util/paramset.h"
#include <QDateTime>
#include "host.h"
#include "util/paramsprovider.h"

class TaskRequestData;

class TaskRequest : public ParamsProvider {
  QSharedDataPointer<TaskRequestData> d;
public:
  enum TaskRequestStatus { Queued, Running, Success, Failure, Canceled };
  TaskRequest();
  TaskRequest(const TaskRequest &);
  TaskRequest(Task task, bool force = false);
  ~TaskRequest();
  TaskRequest &operator=(const TaskRequest &);
  bool operator==(const TaskRequest &) const;
  const Task task() const;
  const ParamSet params() const;
  void overrideParam(QString key, QString value);
  quint64 id() const;
  QDateTime submissionDatetime() const;
  QDateTime startDatetime() const;
  void setStartDatetime(QDateTime datetime
                        = QDateTime::currentDateTime()) const;
  void setEndDatetime(QDateTime datetime = QDateTime::currentDateTime()) const;
  QDateTime endDatetime() const;
  qint64 queuedMillis() const {
    QDateTime submission(submissionDatetime());
    return submission.isNull() ? 0 : submission.msecsTo(startDatetime()); }
  qint64 runningMillis() const {
    QDateTime start(startDatetime());
    return start.isNull() ? 0 : start.msecsTo(endDatetime()); }
  qint64 totalMillis() const {
    return submissionDatetime().msecsTo(endDatetime()); }
  qint64 liveTotalMillis() const {
    QDateTime end(endDatetime());
    return submissionDatetime()
        .msecsTo(end.isNull() ? QDateTime::currentDateTime() : end); }
  bool success() const;
  void setSuccess(bool success) const;
  int returnCode() const;
  void setReturnCode(int returnCode) const;
  /** Note that this is the exact target on which the task is running/has been
    * running, i.e. if the task target was a cluster, this is the host which
    * was choosen within the cluster.
    * Return a null Host when the task request is still queued. */
  Host target() const;
  void setTarget(Host target) const;
  QVariant paramValue(const QString key, const QVariant defaultValue) const;
  ParamSet setenv() const;
  void setTask(Task task);
  bool force() const;
  TaskRequestStatus status() const;
  static QString statusAsString(TaskRequest::TaskRequestStatus status);
  QString statusAsString() const {
    return statusAsString(status()); }
  /** @return true iff status != Queued or Running */
  bool finished() const {
    switch(status()) {
    case Queued:
    case Running:
      return false;
    default:
      return true;
    }
  }
  bool isNull();
  QString command() const;
  void overrideCommand(QString command);
  void overrideSetenv(QString key, QString value);
};

uint qHash(const TaskRequest &request);

#endif // TASKREQUEST_H
