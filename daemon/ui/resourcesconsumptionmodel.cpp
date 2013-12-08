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
#include "resourcesconsumptionmodel.h"

ResourcesConsumptionModel::ResourcesConsumptionModel(QObject *parent)
  : TextMatrixModel(parent) {
}

QVariant ResourcesConsumptionModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  switch(role) {
  case TextViews::HtmlPrefixRole:
    // LATER move icons to WebConsole
    if (orientation == Qt::Horizontal)
        return "<i class=\"fa fa-hdd-o\"></i> ";
    if (section > 0) // every line but max line
        return "<i class=\"fa fa-cog\"></i> ";
    return QVariant();
  default:
    return TextMatrixModel::headerData(section, orientation, role);
  }
}

QVariant ResourcesConsumptionModel::data(const QModelIndex &index,
                                         int role) const {
  if (index.row() == 0) {
    switch (role) {
    case TextViews::HtmlPrefixRole:
      return "<strong>";
    case TextViews::HtmlSuffixRole:
      return "</strong>";
    }
  }
  return TextMatrixModel::data(index, role);
}

void ResourcesConsumptionModel::tasksConfigurationReset(
    QHash<QString,TaskGroup> tasksGroups, QHash<QString,Task> tasks) {
  Q_UNUSED(tasksGroups)
  _tasks = tasks.values();
  qSort(_tasks);
}

void ResourcesConsumptionModel::targetsConfigurationReset(
    QHash<QString,Cluster> clusters, QHash<QString,Host> hosts) {
  _clusters = clusters;
  _hosts = hosts.values();
  qSort(_hosts);
}

void ResourcesConsumptionModel::hostResourceConfigurationChanged(
    QHash<QString,QHash<QString,qint64> > resources) {
  _resources = resources;
}

#define MINIMUM_CAPTION "Theorical lowest availability for host"

void ResourcesConsumptionModel::configReloaded() {
  QHash<QString,QHash<QString,qint64> > min = _resources;
  foreach (const Host &host, _hosts)
    setCellValue(MINIMUM_CAPTION, host.id(), QString());
  foreach (const Task &task, _tasks) {
    QStringList targets;
    if (_clusters.contains(task.target()))
      foreach (const Host &host, _clusters.value(task.target()).hosts())
        targets.append(host.id());
    else
      targets.append(task.target());
    foreach (const Host &host, _hosts) {
      if (targets.contains(host.id()) && !task.resources().isEmpty()) {
        QString s;
        foreach (const QString &kind, task.resources().keys()) {
          qint64 consumed = task.resources().value(kind);
          qint64 available = _resources.value(host.id()).value(kind);
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
        setCellValue(task.fqtn(), host.id(), s);
      }
    }
  }
  foreach (const Host &host, _hosts) {
    QString s;
    foreach (const QString &kind, host.resources().keys()) {
      qint64 lowest = min.value(host.id()).value(kind);
      qint64 available = _resources.value(host.id()).value(kind);
      s.append(kind).append(": ").append(QString::number(lowest))
          .append("/").append(QString::number(available)).append(' ');
    }
    if (s.size() > 0)
      s.chop(1);
    setCellValue(MINIMUM_CAPTION, host.id(), s);
  }
}
