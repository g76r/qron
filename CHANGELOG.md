# From 1.8.5 to 1.8.6
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
 - http execution mean and http alerts: it is now possible to follow http
   redirects (although not enabled by default), to log reply content even
   on success, and to validate reply content against a regexp to decide if
   the call was successful regardless the status code
 - requesttask action can now dynamicaly override task params, e.g.:
        - ```(task foo(onsuccess(requesttask bar(params myparam myvalue))))```
 - changed and rationalized many web console uris, e.g.:
        - /console/taskdoc.html?taskid=%1 -> /console/task/%1
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
