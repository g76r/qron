#ifndef RESOURCEALLOCATIONMODEL_H
#define RESOURCEALLOCATIONMODEL_H

#include "textmatrixmodel.h"

class ResourceAllocationModel : public TextMatrixModel {
  Q_OBJECT
public:
  enum Mode { Configured, Allocated, Free, FreeOverConfigured,
              AllocatedOverConfigured };
  static const int HtmlPrefixRole = Qt::UserRole;
  static const int TrClassRole = Qt::UserRole+1;
  static const int LinkRole = Qt::UserRole+2;

private:
  Mode _mode;
  QMap<QString,QMap<QString,qint64> > _configured;

public:
  explicit ResourceAllocationModel(
      QObject *parent = 0, ResourceAllocationModel::Mode mode
      = ResourceAllocationModel::FreeOverConfigured);
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
  void setResourceAllocationForHost(QString host,
                                    QMap<QString,qint64> resources);
  void setResourceConfiguration(QMap<QString,QMap<QString,qint64> > resources);
};

#endif // RESOURCEALLOCATIONMODEL_H
