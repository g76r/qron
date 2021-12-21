# From 1.10.0 to 1.10.1 (2021-12-21)
* Minor improvements
 - task trigger diagram: taskrequest edges constraint layout (again)
 - %-evaluation: introduced %=match
* Bugfixes
 - fixed 2 bugs introduced by task templates implementation
  - params were sent to ssh tasks instead of vars
  - triggers deduced from event subscriptions was no longer visible in
    wui and api

# From 1.9.10 to 1.10.0 (2021-12-14)
* New features and notable changes
 - Based on Qt6, no longer supporting Qt5
 - config: introducing task templates
 - adding new docker execution mean
 - config: taskgroups now inherit from each others
   e.g. taskgroup foo.bar contains params, event subscriptions, etc. of
   taskgroup foo (if taskgroup foo exists)
* Minor improvements and bug fixes
 - config: renaming setenv to vars and removing unsetenv
   also: no longer import qron system environement into tasks ones
 - http api: do not redirect on api calls when setting header
   "Prefer: return=representation"
 - http api: introduced /rest/v1/scheduler/stats.json
 - docker packaging, pushed to public docker hub
 - removed old (4 years old) compatibility wui and api endpoints
 - graphviz diagrams: densify diagrams with ranksep=0
 - alert: made html alert xhtml compliant (or at less close markups)
   otherwise some simple html renderers (such as teams and its
   smtp-to-channel gateway) display garbage
 - %-evaluation: introduced %={sha1,md5,sha256,hex,fromhex,base64,frombase64}
 - %-evaluation: introduced %=eval %=escape and fixed %=env
 - linux packaging: integrating Qt-6.2 libs until all distribs provide them
 - webconsole: fixed support for headers in html templating engine
   this now works: <?value:!header Host?>
 - webconsole: hide navbar on hidenav=true parameter or cookie
 - task trigger diagram: improvement incl. display action params
* Bugfixes
 - %-evaluation: fixed %=sub which discarded data after regexp match
   %{=sub!food!/o/O} used to return "fO" instead of "fOod"
   whereas %{=sub!food!/o/O/g} correctly returned "fOOd"
 - webconsole: allow access to /font w/o credentials
 - http api: fixed last logs endpoints
   fixed:
     /rest/v1/logs/last_info_entries.csv
   added:
     /rest/v1/logs/last_audit_entries.csv
     /rest/v1/logs/last_warning_entries.csv
     /rest/v1/logs/last_warning_entries.html
* Behind-the-curtain improvements
 - many code fixes due to warnings from new compilers versions introduced
   with Qt6

# From 1.9.9 to 1.9.10 (2021-11-07)
* Minor improvements and bug fixes
 - alerts: SMTP network timeout is no longer 2" but 5" by default, and can be
     set using new alert param ```param mail.smtp.timeout```
 - action: added 'writefile' action and fixed 'requesturl' action's params
     e.g.
     ```(writefile(path /tmp/custom_file.txt)"%!taskid finished\n")```
     ```(writefile(path /tmp/custom_file_XXXXXX.txt)(append false)(truncate true)(unique true)"%!taskid\n")```
     ```(requesturl (address http://localhost/test2)(user bar)(password password)(method post))```
 - action: added taskinstance params to %-evaluation context of several actions
     requesturl, writefile, requesttask, postnotice
 - notices: minor fixes in !notice pseudo param handling and doc
 - httpd: normalizing header names: foo-bar: -> Foo-Bar:
     this is mandatory when used behind an AWS load balancer
 - http rest api: added GET /rest/v1/tasks/%taskid/list.csv
 - httpd: CORS support enhancement
 - httpd: 32 workers instead of 8
 - alerts: renaming for consistency: resource.exhausted.* -> task.resource_exhausted.*
 - graphviz diagrams improvement
     display only parent group edges, not every ancestors
     ignore events and notices declared at task level for layout
 - config: %=env function to read system environment variables
 - config: disable all tasks at startup if DISABLE_TASKS_ON_CREATION=y 
 - doc: added parts to user manual
 - linux packaging:
     introduced docker image
     systemd service unit file
     several additional improvements
* Bugfixes
 - rest api: fixed /rest/v1/resources/*.csv which responded almost garbage
 - fixed %-evaluation infinite loop when param foo is defined as %foo
* Behind-the-curtain improvements
 - source-level compatibility with Qt 6, therefore sticking to Qt 5.15 (last Qt 5 LTS)
 - gitlab-ci: added artifacts

# From 1.9.8 to 1.9.9 (2017-04-24)
* Minor improvements and bug fixes
 - wui: don't elide command on task detail page
 - api: disabling pagination on every csv endpoint
 - doc: misc fixes
* Behind-the-curtain improvements
 - portability: backward compatibility down to Qt 5.3 again (Debian 8)
 - more consistant build procedures across platforms
 - wui/api: migrating several endpoints from textview to on-the-fly formatting
     /rest/v1/{tasks,taskgroups,steps}/list.csv
     /rest/v1/alerts_{subscriptions,settings}/list.csv
     /rest/v1/{gridboards,calendars}/list.csv
     /rest/v1/logs/logfiles.csv
 - library libqtssu has been renamed to libp6core

# From 1.9.7 to 1.9.8 (2017-01-18)
* Minor improvements and bug fixes
 - wui: fixed a regression in 1.9.7 where static urls needed auth again

# From 1.9.6 to 1.9.7 (2017-01-18)
* New features and notable changes
 - alerts: visibilitywindow now applies to one-shot alerts in addition to
   stateful ones
 - alerts: introducing acceptabilitywindow to avoid even more nightly alerts
   sample config:
   ```(alerts(settings(pattern myalert)(acceptabilitywindow * * 0-4 * * *)))```
* Minor improvements
 - wui/api: introducing (few) filtered api uris, e.g.:
     /rest/v1/taskinstances/list.csv?status=queued,running
     /rest/v1/taskinstances/list.html?status=running
 - wui: all configuration tables are sorted again (as they used to be prior
   to 1.9.0)
 - wui/api: more actions ported to new more restful url scheme:
     /do/v1/taskinstances/{abort,cancel,cancel_or_abort}/%taskinstanceid
     /do/v1/tasks/{enable_all,disable_all}
     /do/v1/tasks/{enable,disable}/%taskid
     /do/v1/alerts/{emit}/%alertid
     /do/v1/alerts/{raise,cancel,raise_immediately,cancel_immediately}/%alertid
     /do/v1/gridboards/clear/%gridboardid
     /do/v1/configs/reload_config_file
     /do/v1/configs/{activate,remove}/%configid
 - code review and fixes after running cppcheck static code analysis code
   (should not have any effect on program behavior, but only on code
   readability/maintenability)
 - wui: searching within log files now work if the log file name patterns uses
   nested % expression (e.g. /var/log/foobar-%{foo%{bar}}.log)
 - wui: added html form to manualy post a notice on events page
 - log: internal Qt traces that are sent to stderr are now timestamped and
   have the same format than qron's log
 - doc: added parts to user manual
 - log: clarified timestamps in ending task info trace
 - log/wui/api: changed audit logs action names to match new restful api paths
   (and methods) rather than c++ methods names, e.g. "raiseAlert" becomes
   "GET /do/v1/alerts/raise/"
* Behind-the-curtain improvements
 - switched more regular expression used internaly from Qt4's QRegExp to Qt5's
   QRegularExpression, which is richer, more performant and will last longer,
   see 1.9.0 changelog below)
 - renamed some c++ methods names to improved consistency with status names
   in documentation (submission -> request, etc.)
 - wui/api: removed some wui/api endpoints that are no longer used (in do? and
   confirm? pseudo-pages)
 - wui/api: access control supporting new restful url schemes
 - wui/api: several api endpoints have been migrated from textviews to
   on-the-fly formatting
 - wui: new confirmation url pattern, more restfull:
   "/confirm/path/to/actual/api/" instead of "/confirm?event=apiCallName"
 - wui/api: deprecation debug traces for old wui and api urls (do?, old
   confirm?, taskdoc.html and gridboard.html)

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
