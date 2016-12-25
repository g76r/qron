# Since 1.9.6
* New features and notable changes
 - alerts: visibilitywindow now applies to one-shot alerts in addition to
   stateful ones
 - alerts: introducing acceptabilitywindow to avoid even more nightly alerts
   sample config:
   ```(alerts(settings(pattern myalert)(acceptabilitywindow * * 0-4 * * *)))```
* Minor improvements
 - wui: all configuration tables are sorted again (as they used to be prior
   to 1.9.0)
 - wui/api: more actions ported to new more restful url scheme:
     /do/v1/taskinstances/abort/%taskinstanceid
     /do/v1/taskinstances/cancel/%taskinstanceid
     /do/v1/taskinstances/cancel_or_abort/%taskinstanceid
 - code review and fixes after running cppcheck static code analysis code
   (should not have any effect on program behavior, but only on code
   readability/maintenability)
 - wui: searching within log files now work if the log file name patterns uses
   nested % expression (e.g. /var/log/foobar-%{foo%{bar}}.log)
 - log: internal Qt traces that are sent to stderr are now timestamped and
   have the same format than qron's log
 - doc: added parts to user manual
 - log: clarified timestamps in ending task info trace
* Behind-the-curtain improvements
 - switched more regular expression used internaly from Qt4's QRegExp to Qt5's
   QRegularExpression, which is richer, more performant and will last longer,
   see 1.9.0 changelog below)
 - renamed some c++ methods names to improved consistency with status names
   in documentation (submission -> request, etc.)

# From 1.9.5 to 1.9.6 (2016-11-08)
* Minor improvements and fixes
 - wui: added /do/v1/tasks/abort_instances/%taskid
   /do/v1/tasks/cancel_requests/%taskid and
   /do/v1/tasks/cancel_requests_and_abort_instances/%taskid
   api endpoints
 - wui: added version number on overview and about pages
 - dependencies upgrade (libqtpf, libbqtssu)

# From 1.9.4 to 1.9.5 (2016-09-12)
* Bugfixes
 - fixed enqueueall/enqueueanddiscardqueued queuing policies inversion
   version 1.9.3 did mess up the two policies, and moreover introduce a
   regression in default policy
* Minor improvements
 - wui: more user manual links from task details page
 - wui: added queueing policy to task detail page
 - wui: fixed taskinstance queued status label

# From 1.9.3 to 1.9.4 (2016-09-08)
* Bugfixes
 - fixed regression automatic taskinstance date params

# From 1.9.2 to 1.9.3 (2016-09-07)
* New features and notable changes
 - scheduler: introducing task enqueuepolicy parameter, which replaces and
   enhance former discardaliasesonstart
   see http://qron.eu/doc/master/user-manual.html#enqueuepolicy
 - alerts: introducing visibilitywindow to e.g. avoid nightly alerts
   sample config:
   ```(alerts(settings(pattern myalert)(visibilitywindow * * 8-22 * * *)))```
* Minor improvements and fixes
 - wui: when a request form declares an external allowed values list which
   cannot be fetched (e.g. a remote http list whose http server did not
   respond), the request form displays a clarified error messages and
   submitting is disabled
 - rewritten config file now escapes backslashes within double quotes the
   right way (the rewritten file was no longer consistent with original since
   1.9.0)
 - wui: pf fragment was incorrectly %-evaluated on task details page
 - wui: it was no longer possible to set custom titles since 1.9.0, fixed
 - config: fixed a config parser leading space bug
   e.g. ```(name(child) content)``` was parsed as if it were
   ```(name(child)' content')```
 - wui: fixed "Recently Emitted Alerts" view, which used to update first
   emit line instead of logging all of them once after another
 - config: removed an erroneous warning on config reload
   "WARNING alert channel 'comment' unknown in alert subscription"

# From 1.9.1 to 1.9.2 (2016-07-21)
* Bugfixes
 - restored /rest/do?event=requestTask&taskid= web api entry point

# From 1.9.0 to 1.9.1 (2016-07-18)
* New features and notable changes
 - fixed severe bug in ssh execution mean command line arguments spliting
   that disallowed having a full multi-line shell script in config file
 - introduced a new task param "command.shell" that make it possible to
   choose another execution shell than the default one for ssh and local
   means
 - fully rewritten request form field's 'allowedvalues' to use csv value
   list that preserves config file values order, enables setting a display
   label different from the value; allowedvalues can also be defined in
   a csv file or network resource (such as http://myapp/myvalues/)
* Minor improvements and fixes
 - request form field's 'allowedvalues' now defaults to "" when no default
   value is set
 - fixed a rather rare case where mayrise alerts were raised, hence
   improving signal/noise ratio (when both mayrise and rise delays where
   reached during the same asynchronous state check loop, the alerts was
   always raised, now it's raised only if rise delay was reached before
   mayrise delay)
 - fixed linux packaging where libQt5Sql.so.5 was lacking

# From 1.8.5 to 1.9.0 (2016-07-08)
* New features and notable changes
 - a user manual is now available embeded within web ui and online on the
   Internet, here: http://qron.eu/doc/master/user-manual.html
 - ssh and local execution means: changed space escaping syntax in config
   file's task command (now a backslash protects a space in command line
   tokenization), arguments split can now be protected by backslash, e.g.:
        - ```(command echo 1\\ 2) # only one argument: "1 2"```
        - ```(command echo '1\ 2') # same```
        - ```(command echo "1\\ 2") # same```
 - config file format: backslashes are no longer protected by double quotes,
   enabling such syntax that was not possible before:
   ```"foo \"bar\" \n\u20aC"```
 - clusters have learned two balancing methods: ```random``` and
   ```roundrobin``` (which is now the default one, instead of ```first```)
 - task request form fields can now be declared mandatory, previously all
   of them were optional
        - ```(field dryrun(allowedvalues true false)(mandatory))```
 - task request form fields can now be constrained by an enumeration of
   allowed values in addition to already available regular expression
   pattern
        - ```(field dryrun(suggestion false)(allowedvalues true false))```
 - http execution mean and http alerts: it is now possible to follow http
   redirects (although not enabled by default), to log reply content even
   on success, and to validate reply content against a regexp to decide if
   the call was successful regardless the status code
 - requesttask action can now dynamicaly override task params, e.g.:
        - ```(task foo(onsuccess(requesttask bar(params myparam myvalue))))```
 - changed and rationalized many web console uris, e.g.:
        - /console/taskdoc.html?taskid=%1 -> /console/tasks/%1
        - /console/requestform?taskid=%1 -> /console/tasks/request/%1
        - /console/index.html -> /console/overview.html

* Minor improvements and fixes
 - wui: added nice anchors using AnchorJS and links to the user manual all
   over the wui
 - html exports: better html entities compatiblity: apos -> #39 and
   quote -> #34 this is more portable since apos is not in HTML4 and MS
   Excel did not import it when copy'n pasting
 - wui: fixed a case were task last execution infos (last exec status, last
   exec timestamp...) was not updated (when a task starts during its start
   sequence)
 - httpd: supports for space encoded as + in urls, previsously + in urls were
   interpreted as +, which is incorrect when the http request is performed
   by an html form, and thus spaces in e.g. alert raising or task request
   forms were replaced with +
 - wui: added /console/test.html test page for supervision purposes
 - wui: fixed gridboard content truncate for large gridboards
 - wui: display "no trigger" label and icon on task detail w/o trigger
 - wui: replaced webconsole.cssoverload with webconsole.customhead
   and make it possible to customize more of the html head than the
   stylesheet
 - httpd: X-Fowarded-For w/ several header occurences or custom header
 - httpd: switched from HTTP/1.0 to HTTP/1.1 + Connection: close
 - httpd: send 301 instead of 302 when possible/applicable
 - httpd: no longer ask for authentication (401) for resources that are
   authorized to anyone (which make possible to set up a supervision probe
   without authentication)
 - httpd: no longer send body in response to HEAD request (used to process a
   HEAD as if it was a GET)
 - wui: fixed content truncate when too large in several locations: request
   forms (adhoc.html), pf config field (taskdoc.html) and several customization
   fields (css in header.html and custom buttons in taskdoc.html)
 - changed and rationalized many REST API uris, to have a consistent and more
   standards compliant scheme, e.g.
        - /rest/csv/taskgroups/list/v1 -> /rest/v1/taskgroups/list.csv
        - /rest/svg/tasks/workflow/v1?taskid=%1 -> /rest/v1/tasks/%1/workflow.svg

* Behind-the-curtain improvements
 - wui: several cleanups (removing query string redirect of with cookie values
   for static resources, useless debug traces, etc.)
 - enhanced performances and resources consumption in Qt collection usage
   (mainly QList), for SharedUiItem derived objects (most of qron data classes)
   and PfNode
 - enhanced performances in params evaluation function and http path analyzer
   by switching from if/else/if blocks to hashmaps and radix tree lambda
   methods containers
 - switched most regular expression to richer Perl syntax with a more performant
   engine (switched from Qt's QRegExp to QRegularExpression which is powered by
   PCRE), see [QRegExp](http://doc.qt.io/qt-5/qregexp.html),
   [QRegularExpression](http://doc.qt.io/qt-5/qregularexpression.html),
   [PCRE](https://en.wikipedia.org/wiki/Perl_Compatible_Regular_Expressions) and
   [Comparision of regexp engines]
   (https://en.wikipedia.org/wiki/Comparison_of_regular_expression_engines)
 - reingeniering work in data framework needed by GUI but without notable
   effect on scheduler daemon
 - muted some verbose debug log entries
