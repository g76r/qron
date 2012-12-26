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
#include "taskgroup.h"
#include "paramset.h"
#include <QString>
#include "task.h"
#include "pf/pfnode.h"
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

QString TaskGroup::label() const {
  return d->_label;
}

ParamSet TaskGroup::params() const {
  return d->_params;
}

bool TaskGroup::isNull() const {
  return d->_id.isNull();
}

QDebug operator<<(QDebug dbg, const TaskGroup &taskGroup) {
  dbg.nospace() << taskGroup.id(); // LATER display more
  return dbg.space();
}
