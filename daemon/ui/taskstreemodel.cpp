#include "taskstreemodel.h"
#include  <QtDebug>

#define COLUMNS 6

class TasksTreeModel::TasksTreeItem {
public:
  TasksTreeItem *_parent;
  QString _id, _path;
  int _depth;
  QList<TasksTreeItem*> _children;
  bool _isStructure;

  TasksTreeItem(TasksTreeItem *parent, QString id, QString path, int depth,
                bool isStructure)
    : _parent(parent), _id(id), _path(path), _depth(depth),
      _isStructure(isStructure) {
    if (parent)
      parent->_children.append(this);
  }
  ~TasksTreeItem() {
    qDeleteAll(_children);
  }
  TasksTreeItem *childrenById(const QString id) {
    foreach(TasksTreeItem *c, _children)
      if (c->_id == id)
        return c;
    return 0;
  }
  TasksTreeItem *childrenByPath(const QString path) {
    foreach(TasksTreeItem *c, _children)
      if (c->_id == path)
        return c;
    return 0;
  }
  /** @return -1 if not found */
  int childrenIndex(TasksTreeItem *child) {
    int i = 0;
    foreach(TasksTreeItem *c, _children) {
      if (c == child)
        return i;
      ++i;
    }
    return -1;
  }
};

TasksTreeModel::TasksTreeModel(QObject *parent) : QAbstractItemModel(parent),
  _root(new TasksTreeItem(0, "", "", 0, true)) {
}

TasksTreeModel::~TasksTreeModel() {
  delete _root;
}

QModelIndex TasksTreeModel::index(int row, int column,
                                  const QModelIndex &parent) const {
  if (parent.isValid()) {
    TasksTreeItem *i = (TasksTreeItem*)parent.internalPointer();
    if (i)
      return createIndex(row, column, i->_children.at(row));
  } else if (row >= 0 && row < _root->_children.size())
    return createIndex(row, column, _root->_children.at(row));
  return QModelIndex();
}

QModelIndex TasksTreeModel::parent(const QModelIndex &child) const {
  if (child.isValid()) {
    TasksTreeItem *i = (TasksTreeItem*)child.internalPointer();
    if (i) {
      TasksTreeItem *p = i->_parent;
      if (p)
        return createIndex(p->childrenIndex(i), 0, p);
    }
  }
  return QModelIndex();
}

int TasksTreeModel::rowCount(const QModelIndex &parent) const {
  //qDebug() << "TasksTreeModel::rowCount()" << parent << parent.isValid();
  if (parent.isValid()) {
    TasksTreeItem *i = (TasksTreeItem*)parent.internalPointer();
    if (i)
      return i->_children.size();
  }
  return _root->_children.size();
}

int TasksTreeModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant TasksTreeModel::data(const QModelIndex &index, int role) const {
  //qDebug() << "TasksTreeModel::data()" << index << index.isValid()
  //         << index.internalPointer();
  if (index.isValid()) {
    TasksTreeItem *i = (TasksTreeItem*)(index.internalPointer());
    //qDebug() << "  " << i;
    if (i) {
      //qDebug() << "  " << i << i->_isStructure << i->_id << i->_path;
      if (i->_isStructure) {
        TaskGroup g = _groups.value(i->_path);
        switch(role) {
        case Qt::DisplayRole:
          switch(index.column()) {
          case 0:
            //return QString("%1 %2 %3").arg((int)i).arg(g.id()).arg(i->_path);
            return i->_path;
          case 1:
            return g.label();
          case 5:
            return g.params().toString();
          }
          break;
        case TrClassRole:
          return "";
        case HtmlPrefixRole:
          if (index.column() == 0)
            return "<i class=\"icon-folder-open\"></i> ";
          break;
        default:
          ;
        }
      } else {
        // task
        Task t = _tasks.value(i->_path);
        switch(role) {
        case Qt::DisplayRole:
          switch(index.column()) {
          case 0:
            return i->_id;
          case 1:
            return t.label();
          case 2:
            return t.mean();
          case 3:
            return t.command();
          case 4:
            return t.target();
          case 5:
            return t.params().toString();
          }
          break;
        case HtmlPrefixRole:
          if (index.column() == 0)
            return "<i class=\"icon-cog\"></i> ";
          break;
        default:
          ;
        }
      }
    }
  }
  return QVariant();
}

QVariant TasksTreeModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Id";
    case 1:
      return "Label";
    case 2:
      return "Mean";
    case 3:
      return "Command";
    case 4:
      return "Target";
    case 5:
      return "Parameters";
    }
  }
  return QVariant();
}

TasksTreeModel::TasksTreeItem *TasksTreeModel::getOrCreateItemByPath(
    QString path, bool isStructure) {
  QStringList remainingElements = path.split('.', QString::SkipEmptyParts);
  QStringList doneElements;
  TasksTreeItem *i = _root;
  int depth = 1;
  while (!remainingElements.isEmpty()) {
    QString id = remainingElements.takeFirst();
    doneElements.append(id);
    TasksTreeItem *c = i->childrenById(id);
    //qDebug() << "TasksTreeModel::getOrCreateItemByPath()"
    //         << path << isStructure << c << id << doneElements.join(".");
    i = c ?: new TasksTreeItem(i, id, doneElements.join("."), depth,
                               isStructure);
    ++depth;
  }
  return i;
}

void TasksTreeModel::setAllTasksAndGroups(const QMap<QString,TaskGroup> groups,
                                          const QMap<QString,Task> tasks) {
  beginResetModel();
  QStringList names;
  foreach(QString id, groups.keys())
    names << id;
  names.sort(); // get a sorted groups id list
  foreach(QString id, names)
    getOrCreateItemByPath(id, true);
  names.clear();
  foreach(QString id, tasks.keys())
    names << id;
  names.sort(); // get a sorted groups id list
  foreach(QString id, names)
    getOrCreateItemByPath(id, false);
  _groups = groups;
  _tasks = tasks;
  endResetModel();
  //qDebug() << "TasksTreeModel::setAllTasksAndGroups" << _root->_children.size()
  //         << _groups.size() << _tasks.size()
  //         << (_root->_children.size() ? _root->_children[0]->_id : "null");
}
