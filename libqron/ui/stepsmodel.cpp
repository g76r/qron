/* Copyright 2014 Hallowyn and others.
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
#include "stepsmodel.h"
#include "config/eventsubscription.h"

#define COLUMNS 10

class StepsModel::StepWrapper {
  QString _fqsn, _id, _kindToString, _workflowId;
  Step _source;
  QString _onReadyToString, _predecessors;

public:
  StepWrapper(Step source) // real step
    : _fqsn(source.fqsn()), _id(source.id()),
      _kindToString(source.kindToString()),
      _workflowId(source.workflowId()), _source(source) {
    if (_source.kind() != Step::SubTask)
      _onReadyToString
          = EventSubscription::toStringList(
            _source.onreadyEventSubscriptions()).join('\n');
    bool first = true;
    foreach (QString p, _source.predecessors()) {
      if (first)
        first = false;
      else
        _predecessors.append('\n');
      _predecessors.append(p);
    }
  }
  StepWrapper(Task workflow, bool isStart) // pseudo steps
    : _fqsn(workflow.id()+(isStart ? ":$start" : ":$end")),
      _id(isStart ? "$start" : "$end"),
      _kindToString(isStart ? "start" : "end"), _workflowId(workflow.id()) {
    if (isStart)
      foreach (QString s, workflow.startSteps())
        _onReadyToString.append(" ->").append(s);
    // LATER compute $end predecessors
  }
  QString fqsn() const { return _fqsn; }
  QString id() const { return _id; }
  QString kindToString() const { return _kindToString; }
  QString workflowId() const { return _workflowId; }
  Task subtask() const { return _source.subtask(); }
  QString onReadyToString() const { return _onReadyToString; }
  QString predecessors() const { return _predecessors; }
  //bool operator<(const StepWrapper &other) const { return _fqsn < other._fqsn; }
  //bool operator==(const StepWrapper &other) const {
  //  return _fqsn == other._fqsn;
  //}
};

StepsModel::StepsModel(QObject *parent) : QAbstractTableModel(parent) {
}

StepsModel::~StepsModel() {
}

int StepsModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : _steps.size();
}

int StepsModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return COLUMNS;
}

QVariant StepsModel::data(const QModelIndex &index, int role) const {
  if (index.isValid() && index.row() >= 0 && index.row() < _steps.size()) {
    const StepWrapper &step(_steps.at(index.row()));
    switch(role) {
    case Qt::DisplayRole:
      switch(index.column()) {
      case 0:
        return step.fqsn();
      case 1:
        return step.id();
      case 2:
        return step.kindToString();
      case 3:
        return step.workflowId();
      case 4:
        return step.subtask().id();
      case 5:
        return step.predecessors();
      case 6:
        return step.onReadyToString();
        break;
      case 7:
        return EventSubscription::toStringList(
              step.subtask().onstartEventSubscriptions()).join('\n');
      case 8:
        return EventSubscription::toStringList(
              step.subtask().onsuccessEventSubscriptions()).join('\n');
      case 9:
        return EventSubscription::toStringList(
              step.subtask().onfailureEventSubscriptions()).join('\n');
      }
      break;
    default:
      ;
    }
  }
  return QVariant();
}

QVariant StepsModel::headerData(int section, Qt::Orientation orientation,
                                int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch(section) {
    case 0:
      return "Fully qualified step name";
    case 1:
      return "Id within workflow";
    case 2:
      return "Step kind";
    case 3:
      return "Workflow";
    case 4:
      return "Subtask";
    case 5:
      return "Predecessors";
    case 6:
      return "On ready";
    case 7:
      return "On subtask start";
    case 8:
      return "On subtask success";
    case 9:
      return "On subtask failure";
    }
  }
  return QVariant();
}

void StepsModel::configChanged(SchedulerConfig config) {
  if (!_steps.isEmpty()) {
    beginRemoveRows(QModelIndex(), 0, _steps.size()-1);
    _steps.clear();
    endRemoveRows();
  }
  QList<StepWrapper> steps;
  foreach (const Task task, config.tasks().values()) {
    if (!task.steps().isEmpty()) {
      int row;
      for (row = 0; row < _steps.size(); ++row) {
        const StepWrapper &step2 = _steps.at(row);
        if (task.id() < step2.workflowId())
          break;
      }
      steps.insert(row++, StepWrapper(task, true));
      foreach (const Step step, task.steps())
        steps.insert(row++, step);
      steps.insert(row, StepWrapper(task, false));
    }
  }
  //std::sort(_steps.begin(), _steps.end());
  //qSort(_steps);
  if (!steps.isEmpty()) {
    beginInsertRows(QModelIndex(), 0, steps.size()-1);
    _steps = steps;
    endInsertRows();
  }
}
