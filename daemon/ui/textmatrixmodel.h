#ifndef TEXTTABLEMODEL_H
#define TEXTTABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>
#include <QMap>

class TextMatrixModel : public QAbstractTableModel {
  Q_OBJECT
  QStringList _columnNames, _rowNames;
  QMap<QString,QMap<QString,QString> > _values;

public:
  explicit TextMatrixModel(QObject *parent = 0);
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  QString value(const QString row, const QString column) const;

public slots:
  void setCellValue(const QString row, const QString column,
                    const QString value);
};

#endif // TEXTTABLEMODEL_H
