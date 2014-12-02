/* Copyright 2013-2014 Hallowyn and others.
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
#include "requestformfield.h"
#include <QSharedData>
#include <QRegExp>
#include <QPair>
#include "pf/pfnode.h"
#include "log/log.h"
#include "sched/taskinstance.h"
#include "configutils.h"
#include "util/htmlutils.h"

class RequestFormFieldData : public QSharedData {
public:
  QString _param, _label, _placeholder, _suggestion;
  QRegExp _format;
  QStringList _appendcommand;
  QList<QPair<QString,QString> > _setenv;
};

RequestFormField::RequestFormField() : d(new RequestFormFieldData) {
}

RequestFormField::RequestFormField(const RequestFormField &rhs) : d(rhs.d) {
}

RequestFormField::RequestFormField(PfNode node) {
  RequestFormFieldData *d = new RequestFormFieldData;
  QStringList sl = node.stringChildrenByName("param");
  if (sl.size() != 1) {
    Log::error() << "request form field "
                 << (sl.isEmpty() ? "without param value: "
                                  : "several param values: ")
                 << node.toString();
    return;
  }
  d->_param = ConfigUtils::sanitizeId(sl.first());
  d->_label = node.attribute("label", d->_param);
  d->_placeholder = node.attribute("placeholder", d->_label);
  d->_suggestion = node.attribute("suggestion");
  QString format = node.attribute("format");
  d->_format = QRegExp(format);
  if (!d->_format.isValid()) {
    Log::error() << "request form field with invalid format specification: "
                 << node.toString();
    return;
  }
  d->_appendcommand = node.stringChildrenByName("appendcommand");
  d->_setenv = node.stringsPairChildrenByName("setenv");
  this->d = d;
  //Log::fatal() << "RequestFormField: " << node.toString() << " " << toHtml();
}

RequestFormField::~RequestFormField() {
}

RequestFormField &RequestFormField::operator=(const RequestFormField &rhs) {
  if (this != &rhs)
    d.operator=(rhs.d);
  return *this;
}

QString RequestFormField::toHtmlFormFragment(QString inputClass) const {
  if (!d)
    return QString();
  return "<div class=\"control-group\">\n"
      "  <label class=\"control-label\" for=\""+d->_param+"\">"+d->_label
      +"</label>\n"
      "  <div class=\"controls\">\n"
      "    <input id=\""+d->_param+"\" name=\""+d->_param+"\" type=\"text\" "
      "placeholder=\""+d->_placeholder+"\" value=\""+d->_suggestion+"\""
      "class=\""+inputClass+"\">\n"
      "  </div>\n"
      "</div>\n";
}

QString RequestFormField::toHtmlHumanReadableDescription() const {
  QString v;
  if (d) {
    v = "<p>"+HtmlUtils::htmlEncode(d->_param)+":<dl class=\"dl-horizontal\">";
    if (!d->_label.isEmpty())
      v.append("<dt>label</dt><dd>").append(HtmlUtils::htmlEncode(d->_label))
          .append("</dd>");
    if (!d->_suggestion.isEmpty())
      v.append("<dt>suggestion</dt><dd>")
          .append(HtmlUtils::htmlEncode(d->_suggestion)).append("</dd>");
    if (!d->_placeholder.isEmpty())
      v.append("<dt>placeholder</dt><dd>")
          .append(HtmlUtils::htmlEncode(d->_placeholder)).append("</dd>");
    if (d->_format.isValid())
      v.append("<dt>format</dt><dd>")
          .append(HtmlUtils::htmlEncode(d->_format.pattern())).append("</dd>");
    if (!d->_setenv.isEmpty()) {
      v.append("<dt>setenv</dt><dd>");
      for (QListIterator<QPair<QString,QString> > i(d->_setenv); i.hasNext();) {
        QPair<QString,QString> p(i.next());
        v.append(HtmlUtils::htmlEncode(p.first)).append("=")
            .append(HtmlUtils::htmlEncode(p.second)).append(" ");
      }
      v.append("</dd>");
    }
    if (!d->_appendcommand.isEmpty())
      v.append("<dt>appendcommand</dt><dd>")
          .append(HtmlUtils::htmlEncode(d->_appendcommand.join(" ")))
          .append("</dd>");
    v.append("</dl>");
  }
  return v;
}

bool RequestFormField::validate(QString value) const {
  return d && d->_format.exactMatch(value);
}

void RequestFormField::apply(QString value, TaskInstance *request) const {
  if (request && d && !value.isNull()) {
    request->overrideParam(d->_param, value);
    QString command(request->command());
    foreach (QString s, d->_appendcommand)
      command.append(' ').append(s);
    request->overrideCommand(command);
    for (QListIterator<QPair<QString,QString> > i(d->_setenv); i.hasNext(); ) {
      QPair<QString,QString> p(i.next());
      request->overrideSetenv(p.first, p.second);
    }
  }
}

bool RequestFormField::isNull() const {
  return !d;
}

QString RequestFormField::param() const {
  return d ? d->_param : QString();
}

QString RequestFormField::format() const {
  return d ? d->_format.pattern() : QString();
}
