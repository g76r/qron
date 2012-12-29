#include "textmatrixmodel.h"

TextMatrixModel::TextMatrixModel(QObject *parent) : QAbstractTableModel(parent) {
}

int TextMatrixModel::rowCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return _rowNames.size();
}

int TextMatrixModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return _columnNames.size();
}

QVariant TextMatrixModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _rowNames.size()
      && index.column() >= 0 && index.column() < _columnNames.size()) {
    switch (role) {
    case Qt::DisplayRole:
      return _values.value(_rowNames.at(index.row()))
          .value(_columnNames.at(index.column()));
    }
  }
  return QVariant();
}

QVariant TextMatrixModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
  switch (orientation) {
  case Qt::Horizontal:
    if (section >= 0 && section < _columnNames.size()) {
      switch (role) {
      case Qt::DisplayRole:
        return _columnNames.at(section);
      }
    }
    break;
  case Qt::Vertical:
    if (section >= 0 && section < _rowNames.size()) {
      switch (role) {
      case Qt::DisplayRole:
        return _rowNames.at(section);
      }
    }
  }
  return QVariant();
}

QString TextMatrixModel::value(const QString row, const QString column) const {
  return _values.value(row).value(column);
}

void TextMatrixModel::setCellValue(const QString row, const QString column,
                               const QString value) {
  if (!_rowNames.contains(row)) {
    int pos = _rowNames.size();
    beginInsertRows(QModelIndex(), pos, pos);
    _rowNames.append(row);
    endInsertRows();
  }
  if (!_columnNames.contains(column)) {
    int pos = _columnNames.size();
    beginInsertColumns(QModelIndex(), pos, pos);
    _columnNames.append(column);
    endInsertColumns();
  }
  QMap<QString,QString> values = _values.value(row);
  values.insert(column, value);
  _values.insert(row, values);
  QModelIndex i = index(_rowNames.indexOf(row),
                        _columnNames.indexOf(column));
  emit dataChanged(i, i);
}
