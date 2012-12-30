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
#ifndef TREEMODELWITHSTRUCTURE_H
#define TREEMODELWITHSTRUCTURE_H

#include <QAbstractItemModel>
#include "textviews.h"

class TreeModelWithStructure : public QAbstractItemModel {
  Q_OBJECT
protected:
  class TreeItem;
  TreeItem *_root;

public:
  explicit TreeModelWithStructure(QObject *parent = 0);
  ~TreeModelWithStructure();
  QModelIndex index(int row, int column, const QModelIndex &parent) const;
  QModelIndex parent(const QModelIndex &child) const;
  int rowCount(const QModelIndex &parent) const;
  QModelIndex indexByPath(const QString path) const;

protected:
  TreeItem *getOrCreateItemByPath(QString path, bool isStructure);
  QModelIndex indexByItem(TreeItem *item) const;
};

class TreeModelWithStructure::TreeItem {
public:
  TreeItem *_parent;
  QString _id, _path;
  int _depth;
  QList<TreeItem*> _children;
  bool _isStructure;

  TreeItem(TreeItem *parent, QString id, QString path, int depth,
                bool isStructure)
    : _parent(parent), _id(id), _path(path), _depth(depth),
      _isStructure(isStructure) {
    if (parent)
      parent->_children.append(this);
  }
  ~TreeItem() {
    qDeleteAll(_children);
  }
  TreeItem *childrenById(const QString id) {
    foreach(TreeItem *c, _children)
      if (c->_id == id)
        return c;
    return 0;
  }
  TreeItem *childrenByPath(const QString path) {
    foreach(TreeItem *c, _children)
      if (c->_id == path)
        return c;
    return 0;
  }
  /** @return -1 if not found */
  int childrenIndex(const TreeItem *child) {
    int i = 0;
    foreach(TreeItem *c, _children) {
      if (c == child)
        return i;
      ++i;
    }
    return -1;
  }
  /** @return 0 if not found */
  TreeItem *descendantByPath(const QString path) {
    // MAYDO optimize by decomposing path
    if (_path == path)
      return this;
    foreach (TreeItem *c, _children) {
      TreeItem *i = c->descendantByPath(path);
      if (i)
        return i;
    }
    return 0;
  }
};

#endif // TREEMODELWITHSTRUCTURE_H
