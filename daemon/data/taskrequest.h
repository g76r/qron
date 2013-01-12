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

class TaskRequestData;

class TaskRequest {
  QSharedDataPointer<TaskRequestData> d;
public:
  TaskRequest();
  TaskRequest(const TaskRequest &);
  TaskRequest(Task task, ParamSet params);
  ~TaskRequest();
  TaskRequest &operator=(const TaskRequest &);
  const Task task() const;
  const ParamSet params() const;
  quint64 id() const;
  QDateTime submissionDatetime() const;
  QDateTime startDatetime() const;
  void setStartDatetime(QDateTime datetime
                        = QDateTime::currentDateTime()) const;
  void setEndDatetime(QDateTime datetime = QDateTime::currentDateTime()) const;
  QDateTime endDatetime() const;
  quint64 queuedMillis() const {
    return submissionDatetime().msecsTo(startDatetime()); }
  quint64 runningMillis() const {
    return startDatetime().msecsTo(endDatetime()); }
  quint64 totalMillis() const {
    return submissionDatetime().msecsTo(endDatetime()); }
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
};

#endif // TASKREQUEST_H
