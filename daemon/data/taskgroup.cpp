#include "taskgroup.h"
#include "paramset.h"
#include <QString>

class TaskGroupData : public QSharedData {
  QString _id, _label;
  ParamSet _params;

public:
  TaskGroupData() { }
};

TaskGroup::TaskGroup() {
}

TaskGroup::~TaskGroup() {
}
