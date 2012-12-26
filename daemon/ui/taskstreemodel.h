#ifndef TASKSTREEMODEL_H
#define TASKSTREEMODEL_H

#include <QObject>
#include <QAbstractItemModel>
#include "data/taskgroup.h"
#include "data/task.h"

class TasksTreeModel : public QAbstractItemModel {
  Q_OBJECT
  class TasksTreeItem;
  TasksTreeItem *_root;
  QMap<QString,TaskGroup> _groups;
  QMap<QString,Task> _tasks;

public:
  explicit TasksTreeModel(QObject *parent = 0);
  ~TasksTreeModel();
  QModelIndex index(int row, int column, const QModelIndex &parent) const;
  QModelIndex parent(const QModelIndex &child) const;
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;

public slots:
  void setAllTasksAndGroups(const QMap<QString,TaskGroup> groups,
                            const QMap<QString,Task> tasks);

private:
  TasksTreeItem *getOrCreateItemByPath(QString path, bool isStructure);
};

#endif // TASKSTREEMODEL_H
