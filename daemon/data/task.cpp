#include "task.h"
#include "taskgroup.h"
#include <QString>
#include <QMap>
#include "pfnode.h"
#include "crontrigger.h"
#include "log/log.h"

class TaskData : public QSharedData {
public:
  QString _id, _label, _mean, _command, _target;
  TaskGroup _group;
  ParamSet _params;
  QSet<QString> _eventTriggers;
  QMap<QString,qint64> _resources;
  quint32 _maxtaskinstance;
  QList<CronTrigger> _cronTriggers;

  TaskData() { }
  TaskData(const TaskData &other) : QSharedData(), _id(other._id),
    _label(other._label), _mean(other._mean), _command(other._command),
    _target(other._target), _group(other._group), _params(other._params),
    _eventTriggers(other._eventTriggers), _resources(other._resources),
    _maxtaskinstance(other._maxtaskinstance) { }
  ~TaskData() { }
};

Task::Task() : d(new TaskData) {
}

Task::Task(const Task &other) : d(other.d) {
}

Task::Task(PfNode node) {
  TaskData *td = new TaskData;
  td->_id = node.attribute("id"); // LATER check uniqueness
  td->_label = node.attribute("label", td->_id);
  td->_mean = node.attribute("mean"); // LATER check validity
  td->_command = node.attribute("command");
  td->_target = node.attribute("target");
  td->_maxtaskinstance = node.attribute("maxtaskinstance", "1").toInt();
  if (td->_maxtaskinstance <= 0)
    td->_maxtaskinstance = 1; // LATER warn
  foreach (PfNode child, node.childrenByName("param")) {
    QString key = child.attribute("key");
    QString value = child.attribute("value");
    if (key.isNull() || value.isNull()) {
      // LATER warn
    } else {
      Log::debug() << "configured task param " << key << "=" << value
                   << "for task '" << td->_id << "'";
      td->_params.setValue(key, value);
    }
  }
  foreach (PfNode child, node.childrenByName("trigger")) {
    QString event = child.attribute("event");
    if (!event.isNull()) {
      td->_eventTriggers.insert(event);
      Log::debug() << "configured event trigger '" << event << "' on task '"
                   << td->_id << "'";
      continue;
    }
    QString cron = child.attribute("cron");
    if (!cron.isNull()) {
      CronTrigger trigger(cron);
      if (trigger.isValid()) {
        td->_cronTriggers.append(trigger);
        Log::debug() << "configured cron trigger '" << cron << "' on task '"
                     << td->_id << "'";
      } else
        Log::warning() << "ignoring invalid cron trigger '" << cron
                       << "' parsed as '" << trigger.parsedCronExpression()
                       << "' on task '" << td->_id;
      continue;
      // LATER read misfire config
    }
  }
  // TODO resources
  d = td;
}

Task::~Task() {
}

Task &Task::operator =(const Task &other) {
  if (this != &other)
    d.operator=(other.d);
  return *this;
}

const ParamSet Task::params() const {
  return d->_params;
}

bool Task::isNull() const {
  return d->_id.isNull();
}

const QSet<QString> Task::eventTriggers() const {
  return d->_eventTriggers;
}

QString Task::id() const {
  return d->_id;
}

QString Task::fqtn() const {
  return d->_group.id()+"."+d->_id;
}

QString Task::label() const {
  return d->_label;
}

QString Task::mean() const {
  return d->_mean;
}

QString Task::command() const {
  return d->_command;
}

QString Task::target() const {
  return d->_target;
}

void Task::setTaskGroup(TaskGroup taskGroup) {
  d->_group = taskGroup;
}

const QList<CronTrigger> Task::cronTriggers() const {
  return d->_cronTriggers;
}

QDebug operator<<(QDebug dbg, const Task &task) {
  dbg.nospace() << task.fqtn(); // LATER display more
  return dbg.space();
}
