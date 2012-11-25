#include "taskgroup.h"
#include "paramset.h"
#include <QString>
#include "task.h"
#include "pfnode.h"
#include <QtDebug>

class TaskGroupData : public QSharedData {
  friend class TaskGroup;
  QString _id, _label;
  ParamSet _params;
  //QList<Task> _tasks;
public:
  TaskGroupData() { }
  TaskGroupData(const TaskGroupData &other) : QSharedData(), _id(other._id),
    _label(other._label), _params(other._params)/*, _tasks(other._tasks)*/ { }
  ~TaskGroupData() { }
};

TaskGroup::TaskGroup() : d(new TaskGroupData) {
}

TaskGroup::TaskGroup(const TaskGroup &other) : d(other.d) {
}

TaskGroup::TaskGroup(PfNode node) {
  TaskGroupData *tgd = new TaskGroupData;
  tgd->_id = node.attribute("id"); // LATER check uniqueness
  tgd->_label = node.attribute("label", tgd->_id);
  foreach (PfNode child, node.childrenByName("param")) {
    QString key = child.attribute("key");
    QString value = child.attribute("value");
    if (key.isNull() || value.isNull()) {
      // LATER warn
    } else {
      Log::debug() << "configured taskgroup param " << key << "=" << value
                   << "for taskgroup '" << tgd->_id << "'";
      tgd->_params.setValue(key, value);
    }
  }
  d = tgd;
}

TaskGroup::~TaskGroup() {
}

TaskGroup &TaskGroup::operator =(const TaskGroup &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

//QList<Task> TaskGroup::tasks() {
//  return d->_tasks;
//}

QString TaskGroup::id() const {
  return d->_id;
}

bool TaskGroup::isNull() const {
  return d->_id.isNull();
}

QDebug operator<<(QDebug dbg, const TaskGroup &taskGroup) {
  dbg.nospace() << taskGroup.id(); // LATER display more
  return dbg.space();
}
