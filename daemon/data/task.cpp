#include "task.h"
#include "paramset.h"
#include "taskgroup.h"
#include <QString>
#include <QSet>
#include <QMap>

class TaskData : public QSharedData {
  QString _id, _label;
  TaskGroup _group;
  ParamSet _params;
  QSet<QString> _eventTriggers;
  QMap<QString,quint64> _resources;
  QString _target;

public:
  TaskData() { }
};

Task::Task() {
}

Task::~Task() {
}
