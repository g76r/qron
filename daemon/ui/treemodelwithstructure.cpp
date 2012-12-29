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
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "treemodelwithstructure.h"
#include <QStringList>

TreeModelWithStructure::TreeModelWithStructure(QObject *parent)
  : QAbstractItemModel(parent), _root(new TreeItem(0, "", "", 0, true)) {
}

TreeModelWithStructure::~TreeModelWithStructure() {
  delete _root;
}

QModelIndex TreeModelWithStructure::index(int row, int column,
                                  const QModelIndex &parent) const {
  if (parent.isValid()) {
    TreeItem *i = (TreeItem*)parent.internalPointer();
    if (i)
      return createIndex(row, column, i->_children.at(row));
  } else if (row >= 0 && row < _root->_children.size())
    return createIndex(row, column, _root->_children.at(row));
  return QModelIndex();
}

QModelIndex TreeModelWithStructure::parent(const QModelIndex &child) const {
  if (child.isValid()) {
    TreeItem *i = (TreeItem*)child.internalPointer();
    if (i) {
      TreeItem *p = i->_parent;
      if (p)
        return createIndex(p->childrenIndex(i), 0, p);
    }
  }
  return QModelIndex();
}

int TreeModelWithStructure::rowCount(const QModelIndex &parent) const {
  //qDebug() << "TasksTreeModel::rowCount()" << parent << parent.isValid();
  if (parent.isValid()) {
    TreeItem *i = (TreeItem*)parent.internalPointer();
    if (i)
      return i->_children.size();
  }
  return _root->_children.size();
}

TreeModelWithStructure::TreeItem *TreeModelWithStructure::getOrCreateItemByPath(
    QString path, bool isStructure) {
  QStringList remainingElements = path.split('.', QString::SkipEmptyParts);
  QStringList doneElements;
  TreeItem *i = _root;
  int depth = 1;
  while (!remainingElements.isEmpty()) {
    QString id = remainingElements.takeFirst();
    doneElements.append(id);
    TreeItem *c = i->childrenById(id);
    //qDebug() << "TasksTreeModel::getOrCreateItemByPath()"
    //         << path << isStructure << c << id << doneElements.join(".");
    i = c ?: new TreeItem(i, id, doneElements.join("."), depth,
                               isStructure);
    ++depth;
  }
  return i;
}

