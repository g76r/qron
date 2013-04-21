/* Copyright 2013 Hallowyn and others.
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
#ifndef REQUESTFORMFIELD_H
#define REQUESTFORMFIELD_H

#include <QSharedDataPointer>
#include <QStringList>

class RequestFormFieldData;
class PfNode;
class TaskRequest;
class ParamSet;

class RequestFormField {
  QSharedDataPointer<RequestFormFieldData> d;

public:
  RequestFormField();
  RequestFormField(const RequestFormField &);
  RequestFormField(PfNode node);
  RequestFormField &operator=(const RequestFormField &);
  ~RequestFormField();
  QString toHtml(QString inputClass = "") const;
  bool validate(QString value) const;
  void apply(QString value, TaskRequest *request) const;
  bool isNull() const;
  QString param() const;
  QString format() const;
};

#endif // REQUESTFORMFIELD_H
