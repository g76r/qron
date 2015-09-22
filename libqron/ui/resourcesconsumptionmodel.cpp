/* Copyright 2012-2015 Hallowyn and others.
 * This file is part of qron, see <http://qron.eu/>.
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
#include "resourcesconsumptionmodel.h"

ResourcesConsumptionModel::ResourcesConsumptionModel(QObject *parent)
  : TextMatrixModel(parent) {
}

#define MINIMUM_CAPTION "Theorical lowest availability for host"

void ResourcesConsumptionModel::configChanged(SchedulerConfig config) {
  QList<Task> tasks = config.tasks().values();
  QHash<QString,Cluster> clusters = config.clusters();
  QList<Host> hosts = config.hosts().values();
  QHash<QString,QHash<QString,qint64> > configured;
  foreach (const Host &host, config.hosts())
    configured.insert(host.id(), host.resources());
  qSort(tasks);
  qSort(hosts);
  QHash<QString,QHash<QString,qint64> > min = configured;
  foreach (const Host &host, hosts)
    setCellValue(MINIMUM_CAPTION, host.id(), QString());
  foreach (const Task &task, tasks) {
    QStringList targets;
    if (clusters.contains(task.target()))
      foreach (const Host &host, clusters.value(task.target()).hosts())
        targets.append(host.id());
    else
      targets.append(task.target());
    foreach (const Host &host, hosts) {
      if (targets.contains(host.id()) && !task.resources().isEmpty()) {
        QString s;
        foreach (const QString &kind, task.resources().keys()) {
          qint64 consumed = task.resources().value(kind)*task.maxInstances();
          qint64 available = configured.value(host.id()).value(kind);
          s.append(kind).append(": ").append(QString::number(consumed))
              .append(' ');
          if (available > 0) {
            if (min.contains(host.id())) {
              if (min[host.id()].contains(kind)) {
                min[host.id()][kind] -= consumed;
              } else {
                min[host.id()].insert(kind, available-consumed);
              }
            } else {
              QHash<QString,qint64> h;
              h.insert(kind, available-consumed);
              min.insert(host.id(), h);
            }
          }
        }
        if (s.size() > 0)
          s.chop(1);
        setCellValue(task.id(), host.id(), s);
      }
    }
  }
  foreach (const Host &host, hosts) {
    QString s;
    foreach (const QString &kind, host.resources().keys()) {
      qint64 lowest = min.value(host.id()).value(kind);
      qint64 available = configured.value(host.id()).value(kind);
      s.append(kind).append(": ").append(QString::number(lowest))
          .append("/").append(QString::number(available)).append(' ');
    }
    if (s.size() > 0)
      s.chop(1);
    setCellValue(MINIMUM_CAPTION, host.id(), s);
  }
}
