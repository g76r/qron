#include "resourceallocationmodel.h"

ResourceAllocationModel::ResourceAllocationModel(
    QObject *parent, ResourceAllocationModel::Mode mode)
  : TextMatrixModel(parent), _mode(mode) {
}

QVariant ResourceAllocationModel::headerData(
    int section, Qt::Orientation orientation, int role) const {
  switch(role) {
  case HtmlPrefixRole:
    return orientation == Qt::Horizontal ? "<i class=\"icon-glass\"></i> "
                                         : "<i class=\"icon-hdd\"></i> ";
  default:
    return TextMatrixModel::headerData(section, orientation, role);
  }
}

void ResourceAllocationModel::setResourceAllocationForHost(
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

void ResourceAllocationModel::setResourceConfiguration(
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
