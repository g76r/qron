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
#include "resourcesallocationmodel.h"

ResourcesAllocationModel::ResourcesAllocationModel(
    QObject *parent, ResourcesAllocationModel::Mode mode)
  : TextMatrixModel(parent), _mode(mode) {
}

QVariant ResourcesAllocationModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  switch(role) {
  case TextViews::HtmlPrefixRole:
    // glyphicon-celebration glyphicon-fast-food icon-glass
    return orientation == Qt::Horizontal
        ? "<i class=\"glyphicon-fast-food\"></i> "
        : "<i class=\"icon-hdd\"></i> ";
  default:
    return TextMatrixModel::headerData(section, orientation, role);
  }
}

void ResourcesAllocationModel::setResourceAllocationForHost(
    QString host, QMap<QString,qint64> resources) {
  if (_mode != Configured) {
    QMap<QString,qint64> hostConfigured = _configured.value(host);
    foreach (QString kind, resources.keys()) {
      qint64 configured = hostConfigured.value(kind);
      qint64 free = resources.value(kind);
      switch (_mode) {
      case Configured:
        break;
      case Allocated:
        setCellValue(host, kind, QString::number(configured-free));
        break;
      case Free:
        setCellValue(host, kind, QString::number(free));
        break;
      case FreeOverConfigured:
        setCellValue(host, kind, QString::number(free)+" / "
                     +QString::number(configured));
        break;
      case AllocatedOverConfigured:
        setCellValue(host, kind, QString::number(configured-free)+" / "
                     +QString::number(configured));
      }
    }
  }
}

void ResourcesAllocationModel::setResourceConfiguration(
    QMap<QString,QMap<QString,qint64> > resources) {
  _configured = resources;
  foreach (QString host, resources.keys()) {
    QMap<QString,qint64> hostResources = resources.value(host);
    foreach (QString kind, hostResources.keys()) {
      QString configured = QString::number(hostResources.value(kind));
      switch (_mode) {
      case Free:
      case Configured:
        setCellValue(host, kind, configured);
        break;
      case Allocated:
        setCellValue(host, kind, "0");
        break;
      case FreeOverConfigured:
        setCellValue(host, kind, configured+" / "+configured);
        break;
      case AllocatedOverConfigured:
        setCellValue(host, kind, "0 / "+configured);
        break;
      }
    }
  }
}
