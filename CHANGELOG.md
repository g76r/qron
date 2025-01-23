# From 1.16.6 to 1.16.7 (2024-01-23)
Minor improvements:
- wui: user url params instead of cookies for redirection

Behind-the-curtain improvements:
- updated libp6core with incompatible changes in http api

# From 1.16.5 to 1.16.6 (2024-01-20)
Minor improvements:
- webconsole: last instances history depth on task page can now be choosen
  using global param webconsole.htmltables.lastinstancesdepth (default: 10)

Bug fixes:
- webconsole: fixed last instances duration column on task page

# From 1.16.4 to 1.16.5 (2024-01-14)
Behind-the-curtain improvements:
- updated Qt to 6.8.1 (CI and linux packaging)
- updated libp6core (there are fixes in the update that can remove theorical
  crashes)

# From 1.16.3 to 1.16.4 (2024-11-06)
Minor improvements:
- several minor improvement in events/actions processing, including
  ParamsProviderMerger and ParamsProviderMergerRestorer

Bug fixes:
- fixed a severe bug (crash) in onstderr/onstdout events handling
- wui: fixed client side format check in request form with null/empty format
- wui: % was not escaped in command field on task and taskinstance detail pages

# From 1.16.2 to 1.16.3 (2024-11-01)
New features and notable changes:
- new event onnostderr which is triggered when a tasks finishes and it
  never wrote anything to stderr, usefull to simulate pre-1.13.0 task.stderr
  alerts, this way:
  ```
  (onstderr "^Connection to [^ ]* closed\\.$"(stop)) # written by ssh client
  (onstderr(log stderr: %line(severity W)))
  (onstderr(raisealert task.stderr.%!taskid))
  (onnostderr(cancelalert task.stderr.%!taskid))
  ```
- new tasks/taskgroups/tasktemplates views column: 19 "On nostderr"
- new taskinstance view column: 22 "Had stderr"
- new taskinstance pseudoparam: %!hadstderr

Minor improvements:
- event thread optimization (no longer copying every task & taskinstance
  pseudo parameter on every stdout/stderr line)

Bug fixes:
- wui: top-of-page messages are back

Behind-the-curtain improvements
- building with Qt 6.8.0 image
- libp6core update

# From 1.16.1 to 1.16.2 (2024-10-03)
Minor improvements
- wui: adding client side format check on request form fields in addition to
  server side

# From 1.16.0 to 1.16.1 (2024-09-30)
New features and notable changes:
- introduced host params, making them
  - override task params (with a lower priority than trigger/notice/api)
  - usable in host monitor, especially ssh.xxx params like ssh.identity and
    ssh.username
- added requestform new `(field(format(cause)))` config element in order to
  apply format enforcement selectively to some triggering cause only
  defaults to `api|notice`
  the new `(cause)` is a regular expression checked against the triggering cause
  which can be: `api` for api (including wui) triggering, `notice trigger xxx`
  when triggered by notice xxx, `cron trigger 1 2 3 * * *` when triggered by
  a cron trigger with "1 2 3 * * *" cron expression (note that it's the same
  cause than the one displayed on herd diagram edges)
  this means that requestform field formats are no longer checked for
  in cron triggers by default. this is an intended fix: format enforcement is
  for manual input, not on purpose configuration overriding
  especially: one can define a "year" field with format `^[0-9]{4}$` and also
  a cron trigger with empty "year" field or "guess_it" as a special "year"
  value, and this is now the default (i.e. with no format cause filter):
  ```
  (task task1
     (param begin "%{=date:yyyy}")
     (param end "%{=rpn,%begin,1,+}")
     (requestform
        (field begin(format "2[0-9]{3}"(cause .*)))
        (field end(format "2[0-9]{3}")) # default cause is ^api|notice
     )
     (trigger
       (cron * * * * * *(param begin 1970)) # will be accepted (with end=1971)
       (cron * * * * * *(param begin 1970)(param end foobar)) # also ok, end is not checked
       (cron * * * * * *) # will be rejected because begin cause is .* including cron triggers
       (notice hello(param begin 1970)(param end foobar)) # rejected, end is checked for notice triggers
     )
  )
  ```
- requestform field formats are no longer checked versus empty string ("") when
  the trigger or api does not define them, if they are not defined at all they
  are checked versus task param instead, for instance this will be accepted
  whereas it used to be rejected because trigger "lacks" foo overriding:
  ```
  (task task1
     (param year "%{=date:yyyy}")
     (trigger
       (cron * * * * * *) # was rejected until now
       (cron * * * * * *(param year 1970)) # was already accepted
     )
     (requestform
        (field year(format "2[0-9]{3}"))
     )
  )
  ```
- removed broken requestform config features that did not work or did not
  work consistently for years: (allowedvalues), (mandatory)

Minor improvements
- wui: chronograms green line is thick for running tasks (was thin as if it was
  waiting)
- wui: task instance page minor improvement (task id in title and clickable
  parentid in scheduling section)
- wui: added 10 last executed instances on task page
  (and so removed last exec info from Task object)
- wui: clickable task names in Triggers (scheduling) fields for triggers from
  other tasks events (like e.g. onplan)
- added qron pictogram icon files

Bug fixes:
- probably fixed crash on configuration reload, hard to be sure because it's not
  easily reproduced (rare random crash), by fixing at less one race condition
  in TaskInstance setters/deep copy
- sub-tasks of "sub-herds" without queue condition to start
  before their parent even though it has queue conditions
  yes "sub-herd" is not an actual thing, but let's say it's a task
  ("sub-herder") with onplan children (sub-tasks) planned within another
  task (actual herder)
- fixed %=eval that was no longer working for a while (1 year?)
- %!parenttasklocalid started with a dot
- %!taskinstanceid was evaluated as a string rather than an integer

Behind-the-curtain improvements
- migrated GraphvizImageHttpHandler to GraphvizRenderer
- removed TaskInstanceList (was almost using SUIList anyway)
- better support for ccache+precompiled headers
- no longer compiles on GCC 10, GCC >= 11 is now needed (using C++20
  features that are not supported on GCC 10)

# From 1.15.9 to 1.16.0 (2024-07-14)
New features and notable changes:
- added task instance page on web console, including chronograms and live herd
  diagrams
- added task instance chronogram and svg rendered herd diagram to http api:
  ```
  /rest/v1/taskinstances/%1/chronogram.svg
  /rest/v1/taskinstances/%1/herd_diagram.svg
  ```
- added global param webconsole.alertformat to enable decorating alertid on
  alert views (on overview and alerts pages), e.g.:
  ```
  (param webconsole.alertformat '%1%{=match:%1:\.ping\.: <span class="label label-success">ping</span>:}')
  ```

Minor improvements
- wui: task page now display full pf config including applied templates
  config, group hierarchy config and global (tasksroot) config (previously
  only task config was displayed, without templates groups and globals)
- wui: task page gained some usefull fields that were lacking before and linkable
  anchors on its sections
- wui: changed waiting task instance icon from flag-checkered to hourglass-half
  and planned task instance icon from calendar to fa-table-cells-large
- task new automatic params: %!maxqueuedinstances %!info %!mean %!command
  %!resources
- task instance new automatic params: %!childrenids %!configuredtarget, the
  later being a passthrough for task %!target
- many enhancements in herd diagrams, such as including children outside herd

# From 1.15.8 to 1.15.9 (2024-06-06)
Bug fixes:
- fixed crash in plantaskaction when evaluating overriding params

# From 1.15.7 to 1.15.8 (2024-06-05)
New features and notable changes:
- introducing host ssh healthcheck with new automatic alerts (host.down.**)
  and cluster skipping down hosts
  example:
  ```
  (host bilbo
    (hostname bilbo.shire.com)
    (sshhealthcheck true) # same as /bin/true with most hosts and shells
    (healthcheckinterval 120) # 2 minutes (default: 1 minute)
  )
  (cluster shire (hosts bilbo frodo)) # will use frodo when bilbo is unavaillable
  ```
- removing support for "each" cluster balancing method, please start
  batch of tasks on every server using "scatter" mean tasks instead
- added live herd diagrams to http api (dot only, svg and png coming soon):
  ```
  /rest/v1/taskinstances/%1/herd_diagram.dot
  ```
- added 2 new columns to taskinstance list (on API only, they're not
  shown on the web console): 20 parentid (can be distinct from the herdid),
  21 cause (e.g.: "onfailure", "cron trigger (0 1 2 3 * *)", "api")
  (this information is also shown on new herd diagram)
- introducing maxperhost task/tasktemplate param
  automatically create resources on task and every host in order to
  enforce that a task cannot run above this maximum instances number per
  host, e.g. in the following there won't be more than one "sheep" task
  instance running per host at once, so there won't be more than 3 at
  once because only 3 hosts are defined
  ```
  (taskgroup shire)
  (task sheep
    (taskgroup shire)
    (apply sheep)
  )
  (tasktemplate sheep
    (mean ssh)
    (target cluster1)
    (trigger(cron */3 * * * * *))
    (maxinstances 6)
    (maxperhost 1)
    (command "sleep 20")
  )
  (host localhost)
  (host localhost2(hostname localhost))
  (host localhost3(hostname localhost))
  (cluster cluster1(hosts localhost localhost2 localhost3))
  ```

Bug fixes:
- fixed a crash when a cluster has an invalid balancing method
- fixed a bug in config file integer number parser where hexadecimal numbers
  ending with b or B were misinterpreted (due to support for suffixes in
  numbers a final b was interpreted as billion and 0x1b was processed as
  0x1000000000 which does not fit in a 64-bytes integer, so considered
  invalid (and often used as if it were 0)), now 0x1b is evaluated as 27

Behind-the-curtain improvements
- upgrading libp6core (mainly %-evaluation and utf8 optimizations)
- rewritten internal plantask API to associate every task instance with
  an optionnal parent task (the one calling plantask, for instance through
  onplan or onsuccess events, it may be the herder or another task from the
  same herd or from another) and the plan cause (e.g. "onplan",
  "onfailure", "cron trigger (0 1 2 3 * *)", "api")
- more internal APIs switched from utf16 to utf8
- lacking includes and misc clangd warning removal

# From 1.15.6 to 1.15.7 (2024-05-01)
Bug fixes:
- renamed mergestderrintostdout to mergestdoutintostderr the name was
  misleading
- fixed stdout/stderr behavior for ssh mean tasks so that it's the
  same than with mergestdoutintostderr (excepted if ssh.disablepty is
  set to true)
- alerts dropdelay config element was ignored, mayrise delay was used instead
- fixed duplicates log entries and potential crash on conf change
- queued or planned herded tasks stayed forever with nowait herding policy
- force tasks on (balancing each) cluster to be standalone
  otherwise there were inconsistencies and hard to debug locks with
  maxinstance mechanism

Behind-the-curtain improvements
- upgrading libp6core

# From 1.15.5 to 1.15.6 (2024-04-19)
Minor improvements
- changed RPN syntax in %=rpn
  previously: %{=rpn,foo,'bar,@} -> "hellobar" if foo holds "hello"
  now:        %{=rpn,%foo,bar,@} -> "hellobar" if foo holds "hello"

Bug fixes:
- fixing encoding in alert mails
- fixing ignored requestformfield (config loading bug)
- when both tasktemplate and actual task define the same requestformfield,
  only keep the later one (instead of displaying duplicates on request
  page)

Behind-the-curtain improvements
- upgrading libp6core

# From 1.15.4 to 1.15.5 (2024-02-04)
Minor improvements
- some config error detection (duplicate config files nodes: alerts,
  access-control, requestform)

Behind-the-curtain improvements
- removing foreach macro (C++ supports range for for years now)
- removing last Java-ish Qt iterator (only STL-ish iterators and range fors)
- more utf8 in an internal API
- upgrading libp6core (mostly utf8 related)
- switching to Qt 6.5.3

# From 1.15.3 to 1.15.4 (2023-11-21)
Bug fixes
- fixing maxinstances semaphore, which was broken in 1.15.3

Behind-the-curtain improvements
- more utf8 in an internal API

# From 1.15.2 to 1.15.3 (2023-11-19)
Bug fixes
- fixed a crash causes when reloading configuration file (related to a race
  condition in http data views update)
- HTTPD_LOG_POLICY=LogErrorHits no longer logs hits with status 300..399
- scatter mean: fixed regression in var evaluation
  (var foo %foo) was empty instead of taking params from context (from
  params, scatter.regexp...), due too loop detection, vars being (wrongly)
  in %-evaluation context
  e.g.:
  ```
  (param scatter.regexp "(?<foo>.*)")
  (var foo %foo)
  ```
- fixed a crash causes in shutdown sequence (due to a race condition in httpd
  server own shutdown sequence)
- config history is no longer infinite (only keep 1000 last config events)

# From 1.15.1 to 1.15.2 (2023-10-15)
New features and notable changes
- it's now possible to log console http hits using env variables, e.g.
  HTTPD_LOG_POLICY=LogAllHits or HTTPD_LOG_POLICY=LogErrorHits
  defaults to LogErrorHits, can be muted with HTTPD_LOG_POLICY=LogDisable
  log format is defined using HTTPD_LOG_FORMAT which defaults to:
  "HTTP %[http]url %[http]method %[http]status %[http]servicems %[http]clientaddresses"
  see HttpRequest and HttpResponse doc for available [http] values
  (the whole http pipeline processing context is availlable, even variables
  set during the processing)
- alert: removed !alertxxx !xxxdate pseudo params
  one can use regular section name (same names without leading !) instead

Minor improvements
- adding TaskInstance pseudoparams !parenttaskid and !parenttasklocalid
- more details in log when cannot set {header,status} after writing data

Bug fixes
- split after % evaluation options: scatter.input, ssh.options, docker.*
- pages navigation in html table views was broken since 1.15.0

# From 1.15.0 to 1.15.1 (2023-09-28)
Bug fixes
- %=default regression fix: was coalescing on null instead of empty
- notices: fixed regression in notice overriding params
  both postnotice action and notice trigger
  (postnotice(param foo %foo)) and (trigger(notice(param foo %foo))) did not
  take their value from overriding param foo but was empty due too loop
  detection, overriding params being (wrongly) in %-evaluation context

# From 1.14.1 to 1.15.0 (2023-09-27)
New features and notable changes
- duration of a taskinstance is now running time + waiting time, wheras it
  used to be queued time + running time.
  this change applies to %!durationms %!durations (maxexpectedduration)
  (minexpectedduration) (maxdurationbeforeabort) and to deprecated but
  backwardcompatibility availlable %!totalms and %!totals

Minor improvements
- %-evaluation of overriding parameters in trigger declaration (both cron and
  notice triggers) now take values from trigger (or notice) itself but also
  from the whole task-taskgroup-global hierachy
- removing unused %!envvars and %!headervars implicit parameters
- removing %=escape function, adding %=integer
- won't try to build and push docker unless DOCKER_BUILD_ENABLED=1 is set
- using precompiled header files to speed up compilation
- make taskinstance timestamps (creation, queue, start, stop)
  visible in the "task stopped" end log message, which is now logged too
  for donothing mean tasks in addition to all other means
- added new "task finished" log message at the end of waiting time
- log entries source location is now limited to file:line, the function
  is no more logged it was too verbose

Bug fixes
- %-evaluating users and passwords in access-control, at activation time
  until now this was done at config load time, which is too early because
  it's before external params loading

Behind-the-curtain improvements
- upgraded libp6core which had a deep redesign on framework pieces
  responsible for parameters, %-evaluation and config item value objects
- building with Qt 6.5.2 image

# From 1.14.0 to 1.14.1 (2023-07-18)
Minor improvements
- adding again %!totalms %!totals for backward compatibility (qron < 1.12)
- log entries now have source location info e.g. file.cpp:42:myfunction
- added retrieve_aws_secrets shell script for reading aws secrets as csv
  which make it usable for external params:
  ```(externalparams setname(command retrieve_aws_secrets secret_id))```
- %-evaluating users and passwords in access-control config section

Behind-the-curtain improvements
- building with Debian 12 (bookworm) image
- merged libqtpf into libp6core

# From 1.13.13 to 1.14.0 (2023-07-06)
New features and notable changes
- default deduplicate strategy is now to keep first queued instance
  previous behaviour can be used with (deduplicatestrategy keeplast) task
  attribute
- support for externalized parameters through externalparams and %=ext,
  making possible to store passwords in secret vaults like hashicorp vault
  or aws secretsmanager, or in local csv files
  - (externalparams setname(file /path/to/file)) and
    (externalparams setname(command /opt/bin/foo.sh)) config attributes at
    root level
  - %{=ext:setname:key} param substitution function with global named external
    paramset e.g. (externalparams secret(command /opt/bin/getsecrets.sh))

Behind-the-curtain improvements
- building with Qt 6.5.1 image
- replaced QHash with QMap for small collections and QCache for caches
- removing qRegisterMetaType<>() and qMetaTypeId<>() (which are old Qt
  constructs)

# From 1.13.12 to 1.13.13 (2023-05-05)
Minor improvements
- allowing (resource foobar 0) on task definitions to ignore a resource
  consumption defined in a template or group

Behind-the-curtain improvements
- gitlab ci fixes and clarifying makefiles

# From 1.13.11 to 1.13.12 (2023-04-22)
Minor improvements
- introducing onlast finisher/gather task on scatter mean

Bugfixes
- fixed ambiguities in ParamSet QByteArray/QString/char* API
  which broke alerts default param, including default smtp server
- fixed dockefile and qt libs for docker environment build

# From 1.13.10 to 1.13.11 (2023-04-16)
Minor improvements
- wui: deduplicate criterion in place of cancel when on overview page
- wui: toc shortcuts on top of tasks page
- wui: better FonteAwesome support (ttf fonts were lacking)
- wui: adding "beat" animation effect on some icons

Behind-the-curtain improvements
- using QByteArray instead of QString API in libp6core
- switching from Qt 6.3 to Qt 6.5 (LTS)
- wui: upgrading FontAwesome from 6.0 to 6.4

# From 1.13.9 to 1.13.10 (2023-03-11)
Minor improvements
- fixing runningms in end task log entries (was 0 instead of actual value),
  and making every other xxxxms and xxxs timestamp available as a live
  value before its final value
- a task can now be immediately aborted when waiting for a retry

Bugfixes
- the scheduler no longer stops when trying to soft kill a task that is
  waiting for a retry

Behind-the-curtain improvements
- switching from c++17 to c++20

# From 1.13.8 to 1.13.9 (2023-02-20)
Minor improvements
- adding %!deduplicatecriterion and %!rawdeduplicatecriterion pseudoparams
- support for recursive instance params (instanceparam was %-evaluated with
  a context including params but not instance params)
- performance: less object copies in ParamsProvider descendants
  especially in ParamSet's %-evaluation processing
- %=env now %-evaluate env variable values, e.g. FOO=%bar
- %=rpn operators changes: 
  new ~~ operator (cast to integer) <? >? ?? ??* ?- !- ?* !*
  more standard operators !=~ =~ rather than ~= !~=
- logging shutdown trace _before_ shutdown

Bugfixes
- webconsole: fixed dates on top left part of overview page
- %=date no longer mess up absolute timestamps passed as second param either
  (but probably nobody use such a combo in its qron config file)

Behind-the-curtain improvements
- env variable access fixes: only qgetenv(), converting as local8bit
- removed a linux-specific system call in unix signals handling code

# From 1.13.7 to 1.13.8 (2022-11-23)
New features and notable changes
- new % functions in config file
  %=rpn e.g. `%{=rpn,'0x20,x,+}` returns "33.5" if x is "1.5"
  %=trim e.g. `%{=trim:%foo}`

Minor improvements
- removed a race condition in unix signal handler
- switched to FontAwesome 6 alone in offline doc too
- resynchronized offline user-manual with online one, the offline
  manual got stuck in 2021
- fixed random crashes on shutdown
- fixed potential random bugs in unix signals handling

Behind-the-curtain improvements
- optimizations in regexp handling during %-evaluation
- code quality improvements

# From 1.13.6 to 1.13.7 (2022-07-25):
Bugfixes
- wui: supporting again text view page numbers

# From 1.13.5 to 1.13.6 (2022-07-24):
Bugfixes
- spaces in request form fields are now supported
  previously it was supported only when params where send with GET method
  not POST (i.e. in URI, not body) and even this was no longer working
  (probably since Qt 6, because of a change in overriden methods priorities)
- docker default name now includes %!currenttry
  now: "%{!taskid}_%{!taskinstanceid}_%{!currenttry}"
  was: "%{!taskid}_%{!taskinstanceid}"

# From 1.13.4 to 1.13.5 (2022-06-22):
New features and notable changes
- introducing task retries: re-executing task maxtries-1 times until it does
  not fails, waiting pausebetweentries seconds before each retry.
- new config elements for tasks and tasktemplates: maxtries (default: 1),
  pausebetweentries (in seconds, default: 0.0, example: .250)
- tasks/taskgroups/tasktemplates views columns changes:
  new 43 Max tries
  new 44 Pause between tries
- new task pseudoparam: %!maxtries
- new taskinstances pseudoparams: %!currenttry %!remainingtries
- new auto one-shot alert: task.retry.%!taskid, emitted each time a task is
  retried

Minor improvements
- wui: added max tries and pause between tries in tasks view on alerts page

# From 1.13.3 to 1.13.4 (2022-06-07):
New features and notable changes
- introducing exec action
- better vars quoting and new pseudoparams %!envvars & %!headervars
- local,ssh,docker: new param command.hardkill to change default behavior
  default behavior stays true for local and ssh and false for docker but
  can be overrident task by task

# From 1.13.2 to 1.13.3 (2022-06-06):
New features and notable changes
- background execution mean
  example:
  ```
  (mean background)
  (command '
     sleep 20 > /dev/null 2>&1 &
     echo "!qron:(paramappend taskpid $!)(log hello world!)"
  ')
  (statuscommand "kill -0 %taskpid > /dev/null")
  # status command return code: 0=still running 1= succeeded 2+= failed
  (abortcommand "kill %taskpid")
  ```

Minor improvements
- introducing overrideparam and paramappend actions

Behind-the-curtain improvements
- parsing background tasks commands output to execute actions as if
  they were event subscriptions, this way:
  echo "!qron:(paramappend taskpid $!)(log hello world!)"
  this feature may be reused for something else than background tasks

# From 1.13.1 to 1.13.2 (2022-05-30):
Minor improvements
- wui: more/better links to logs on herds and taskinstances views
- wui: switched to FontAwesome 6 alone, instead of a mix of font icons
- wui: updated about page

Bugfixes
- fixed dangling planned tasks on configuration reload

Behind-the-curtain improvements
- removed any taskinstance reference within taskinstance
  it was bad design pattern and made impossible to have both herds and
  configuration switch work together

# From 1.13.0 to 1.13.1 (2022-05-09):
Minor improvements
- http api: /rest/v1/taskinstances/current/list.{csv,html} are now sorted
  by task instance id
- alert: merging gridboards on reload rather than clearing them
- wui: canceled tasks are now in light grey rather than white
- wui: running/waiting tasks are now green and planned blue, this is an
  inversion, it's fare more obvious for most human that green means running
  and blue planned (somewhat frozen) than the contrary, the previous color
  code was historical because planned and waiting status are new, and blue
  was nice for running to avoid wondering if green is success (white)
  or running (formerly: blue), but more status means more colors...

Bugfixes
- replacing task definition on config load for planned tasks, not only
  queued ones (leaded to inconsistencies in live attributes, including
  running tasks count)
- replacing task params (and task groups params, etc.) in queued and planned
  task instances on reload, only other attributes were replaced (not sure
  if it's a recent or very old bug)
- %=default did not work when receiving only one parameter

Behind-the-curtain improvements
- fixed concurrency bug in log framework/circular buffer
- enhanced/fixed compilation warnings (incl. moving from Qt 6.2 to 6.3)
- factorized/cleaned parts of configuration loading code, removing dead code
- overhauling taskinstances collections in scheduler code

# From 1.12.5 to 1.13.0 (2022-03-08):
New features and notable changes
- introducing mergestderrintostdout boolean attribute to process stdout data
  as if it was received on stderr
  available at task, tasktemplate, taskgroup and root levels
- introducing onstderr and onstdout events subscriptions enabling any action
  when a task writes a line to its standard error and output streams
  example:
  ```
  (mergestderrintostdout)
  (onstderr "^info: *(.*)"(log %1(severity I)))
  (onstderr "error"(log %line(severity W)))
  (onstderr "hello (?<name>[^ ]+)"(log greetings to %name(severity I)))
  (onstderr(writefile "%{=date!iso} %!taskid/%!taskinstanceid : ERR %line"\n(path /tmp/qron-%!taskid-%!taskinstanceid.err)))
  (onstdout(writefile "%{=date!iso} %!taskid/%!taskinstanceid : OUT %line"\n(path /tmp/qron-%!taskid-%!taskinstanceid.out)))
  ```
- no more implicit logging of tasks stderr, can now be configured this way:
  (onstderr "^Connection to [^ ]* closed\\.$"(stop)) # written by ssh client
  (onstderr(log stderr: %line(severity W)))
- no more implicit task.stderr.%!taskid alert, can now be configured this way:
  (onstderr(raisealert task.stderr.%!taskid))
  [actually this is not enough, see 1.15.7 version changelog]
- special param "stderrfilter" is no more supported, it can now be achieved
  with stop action in onstderr event subscriptions, this way:
  (onstderr "^Connection to [^ ]* closed\\.$"(stop)) # written by ssh client
- new planning conditions: allstarted anystarted
- the default queueon becomes "allstarted %!parenttaskinstanceid" which is far
  more easy to understand for a naive user than the former "allsuccess
  %!parenttaskinstanceid"
- new builtin alert task.disabled.%taskid for disabled tasks

Minor improvements
- task/tasktemplate new fields: 38: Merge stdout into stderr,
  39: On stderr, 40: On stdout
- wui: adding herd id on taskinstance view
- scatter mean learned "scatter.lone" boolean parameter
- wui: introducing tasks-resources-hosts graphviz diagram
- wui: graphviz diagram are now displayed full-width on large displays
- removed requesttask (use plantask instead, keeping backward compatibility)
- adding stop action to ignore actions declared after it
  example:
  ```
  (onstderr "^Connection to [^ ]* closed\\.$"(stop))
  (onstderr (log stderr: %0 (severity warning)))
  ```
- adding "iso" shortcut date format name for yyyy-MM-ddThh:mm:ss,zzz
  example: %{=date!iso}
- onplan events subscription is now available at root level (until now it
  was only available at task, tasktemplate and taskgroup levels)
- special parameter "disable.alert.stderr" is no more supported since such
  alerts are disabled by default
- new taskinstance pseudoparams: !planneds !plannedms
- wui: better layout on resources page
- wui: gridboards display "ALERT" instead of "ERROR" when an alert is raised
- mail alerts: (mail(address)) now takes a space-separated list of addresses
  it used to take a comma-separated list which is inconsistent with other conventions
- more good practices in default qron.conf (examples/empty1.conf)

Bugfixes
- queued tasks can become hanged/lost when reloading config
  and instance count became false (hence disrupting maxinstances constraint)
- plantask default queue condition: allfinished -> allsuccess
  avoid sub-sub-tasks starting when their parent is canceled
- scatter.input in scatter execution mean did not evaluate before split
  (param scatter.input %list) did not split values inside %list
- alerter: alerts in mayrise status were raised in some rare cases
  (if cancel occurs at the end of rise period, to late for mayrise
  period to finish before visibility date)
- alerter: safer writing to the gridboard circular buffer
  no longer give 10 ms wait time, and log a warning if buffer full
- fixed duration in stop info log message (was 0 instead of duration)
- plantask was not %-evaluating task id to plan
- wui: graphviz trigger diagram: analyzing root level events task by task
  speaking of on{start,success,failure,finish,plan,stderr,stdout} not e.g.
  onconfigload
- wui+api: gridboard clear api call and wui button was broken

Behind-the-curtain improvements
- changed EventSubscription:: and Action::triggerXXX() methods
  using ParamsProviderMerger* instead of ParamSet and no longer rely on
  actions implementation to merge instance params and pseudo params
  into %-evaluation context
- updated examples/massive2*.conf that are used for endurance testing

# From 1.12.4 to 1.12.5 (2022-01-13):
Minor improvements
- plantask action supports (lone) attribute and can start lone tasks
  exactly like requesttask does, excepted toward "each" balancing clusters
  (this last limitation will be fixed later)
- taskinstance duration is computed since queued, no longer since creation
  this is important for maxexpectedduration computing to exclude time planned
  also renamed pseudoparams: !totalms !totals -> !durationms !durations
  !herdtotalms !herdtotals -> !herddurationms !herddurations
  taskinstance view field 14 is renamed Total time -> Duration
- wui: added Time planned to taskinstances view
- wui: keeping 10000 last executed task instances in memory models
  was 100, and this was fast short on taskinstance views

# From 1.12.3 to 1.12.4 (2022-01-13):
New features and notable changes
- auto canceling/aborting of herderd tasks, allowing aborting of waiting tasks

Minor improvements
- wui: added quicksearch on 3 views: unfinishedtasks, herds, recent instances

# From 1.12.2 to 1.12.3 (2022-01-13):
New features and notable changes
- queuingpolicy replaced with maxqueuedinstances and deduplicatecriterion
  maxqueuedinstances equals maxinstances by default, can be set to
  infinite if <= 0, can be set to any number, and is %-evaluated in the
  taskinstance context
  if deduplicatecriterion is set, it's used to test if two instances are
  are duplicate or not, in addition to the taskid, for instance it can
  be used to avoid canceling tasks that handle different business data
  sets, e.g. (deduplicatecriterion %reportid)
- tasks/taskgroups/tasktemplates views columns changes:
    35 Enqueue policy becomes Max queued instances
    new 37 Deduplicate criterion
- introducing scatter execution mean
  example 1:
  ```
     (task work
       (taskgroup production)
       (mean local)
       (command sleep 5)
       (maxinstances 2)
       (maxqueuedinstances 1)
       (deduplicatecriterion %customer)
     )
     (task largebatch
       (taskgroup production)
       (mean scatter)
       (param scatter.input john:12 kevin:6 mariah:44 carrie:1 paula:75 john:12 paul:67 john:12)
       (param scatter.regexp "(?<name>[^:]+):(?<id>.*)")
       (command work)
       (var customer %name)
       (var cid %id)
       (herdingpolicy nofailure) # canceled subtasks are ok
     )
  ```
  example 2, with intermediary task and a dynamic task instance id list:
  ```
     (task work
       (taskgroup production)
       (apply work)
       (maxinstances 2)
       (maxqueuedinstances 1)
       (deduplicatecriterion %customer)
     )
     (task finalwork
       (taskgroup production)
       (apply work)
     )
     (tasktemplate work
       (mean local)
       (command sleep 5)
     )
     (task largebatch
       (taskgroup production)
       (mean scatter)
       (param scatter.input john:12 kevin:6 mariah:44 carrie:1 paula:75 john:12 paul:67 john:12)
       (param scatter.regexp "(?<name>[^:]+):(?<id>.*)")
       (param scatter.paramappend prerequisites %!taskinstanceid)
       (command work)
       (var customer %name)
       (var cid %id)
     )
     (task mixedbatch
       (taskgroup production)
       (mean donothing)
       (onplan
         (plantask largebatch (paramappend prerequisites %!taskinstanceid))
         (plantask finalwork (queuewhen (allfinished %prerequisites)))
             # finalwork will first wait for largebatch
             # then it will wait for every work task because
             # meanwhile largebatch append ids to %prerequisites
       )
       (herdingpolicy nofailure) # canceled subtasks are ok
     )
  ```

Bugfixes
- instanceparam was fixed in 1.12.2 but not overriding params
- planned tasks were not reevaluated at herder stop, which leaded in
  some cases to dandling planned herded tasks

# From 1.12.1 to 1.12.2 (2022-01-12)
Minor improvements
- default queuewhen condition is now (allfinished %!parenttaskinstanceid)
  rather than (true), which means "when parent is finished" if parent is
  an intermediary task or "as soon as herder started" if parent is the
  herder (because the herder task instance id is not in it's herded
  task list and is thus ignored by allfinished condition)
Bugfixes
- instanceparam and overriding params where totaly broken by 1.12.1 changes
- planned tasks must never be enqueued before the herder starts
  it would allow running tasks regardless resources/locks availability

# From 1.12.0 to 1.12.1 (2022-01-12)
New features and notable changes
- introducing onplan event
  this is convenient to declare tasks that work with or without being
  the herder
  example:
  ```
     (task shepherd
       (taskgroup shire)
       (mean donothing)
       (onplan
         (plantask sheep (paramappend sheeps %!taskinstanceid)
             # paramappend is %-evaluated in the sheep context, so it's the
             # sheep's %!taskinstanceid and the appended param "sheep" is in
             # the context of the calling task, shepherd, hence making it
             # possible to maintain a list of child task ids that will be
             # used below
                         (paramappend first %!taskinstanceid))
             # will be queue immediatly because empty queuewhen means true
         (plantask sheep (paramappend sheeps %!taskinstanceid)
                         (queuewhen(anyfinished %first)) )
             # will be queue as soon as first sheep ends
             # because at evaluation time %first will contain the id of first
             # sheep and only this id
         (plantask sheep (paramappend sheeps %!taskinstanceid)
                         (queuewhen(anyfinished %thisisempty)))
             # will be canceled immediatly because condition cannot be met
             # because %thisisempty contains nothing
         (plantask sheep (paramappend sheeps %!taskinstanceid)
                         (queuewhen(allsuccess %sheeps)))
             # will be queue as soon as third is canceled because at this
             # point the condition can no longer be met
         (plantask sheep (paramappend sheeps %!taskinstanceid)
                         (queuewhen(allfinished %sheeps)))
             # will stay planned forever because he waits for himself
             # since at evaluation time %sheeps evaluates to a list of
             # all sheeps ids, including this one
       )
       (onstart
         (requesttask wolf(lone))
             # plantask can be mixted with requesttask, even if lone (out of
             # herd)
       )
     )
  ```
- new tasks/tasktemplates/taskgroups field: 36 On plan

Minor improvements
- paramappend write in herder params rather than parent params
- plantask and requesttask actions now set a %!parenttaskinstanceid
  overriding param if there is a parenttask, this is the parent task,
  which may not be the herder task if it's already an herded one
  this is convenient to declare tasks that work with or without being
  the herder
- conditions: %-evaluation of a condition task instance id list is now
  done not only in herder context, but in herder context, plus, with
  lesser priority, the instance context
  this make it possible to get %!parenttaskinstanceid (see above)
  and thus define a condition that will match an intermediary
  parent task finish and also start with no delay if the parent
  is started standalone and thus is the herder, explanation:
     (queuewhen(allfinish %!parenttaskinstanceid))
  is evaluated to a list of the intermediary parent task if any,
  which runs as soon as its own condition is met and so triggers
  queueing its child task
  and on the other hand if the parent is started by its own, it
  becomes the herder, and thus its id is ignored in the list, since
  its not in its herded tasks list, so the child is queued immediatly
- wui: adding plantask edges in graphviz triggering diagram
- wui: coloring edges in graphviz triggering diagram, according to
  eventtype (onstart: blue, onplan: green...)

# From 1.11.2 to 1.12.0 (2022-01-11)
New features and notable changes
- action: support for (requesttask(paramappend key value))
  parent param "key" is appended (with space and new value) withe "value"
  which is evaluated in child context (child instance params ans pseudo
  params like %!taskinstanceid), followed by event and parent context
  example:
  ```
    (task foo
       (onstart (requesttask bar (paramappend ids %!taskinstanceid))
       (requesttask baz (paramappend ids %!taskinstanceid))
       (log bar and baz ids: %ids)))
  ```
- introducing planned status, plantask action and task queuing conditions
  planned tasks are created without being queued, and will be queued when
  a predefined condition is met
  conditions available so far includes test on status of other tasks in the
  same herd, making it possible to orchestrate tasks depending from each
  other in a declarative way (non workflow-like but preconditions based)
  implicitly, if there is only one queue condition (one "queuewhen" element
  with only one tasks-related child such as "anyfinished" or "nofailure")
  a cancel condition is computed to cancel the planned task as soon as the
  queue condition can no longer be met (for instance "allsuccess" will be
  transformed in "anynonsuccess" as cancel condition)
  example:
  ```
     (task shepherd
       (taskgroup shire)
       (mean donothing)
       (onstart
         (plantask sheep (paramappend sheeps %!taskinstanceid)
             # paramappend is %-evaluated in the sheep context, so it's the
             # sheep's %!taskinstanceid and the appended param "sheep" is in
             # the context of the calling task, shepherd, hence making it
             # possible to maintain a list of child task ids that will be
             # used below
                         (paramappend first %!taskinstanceid))
             # will be queue immediatly because empty queuewhen means true
         (plantask sheep (paramappend sheeps %!taskinstanceid)
                         (queuewhen(anyfinished %first)) )
             # will be queue as soon as first sheep ends
             # because at evaluation time %first will contain the id of first
             # sheep and only this id
         (plantask sheep (paramappend sheeps %!taskinstanceid)
                         (queuewhen(anyfinished %thisisempty)))
             # will be canceled immediatly because condition cannot be met
             # because %thisisempty contains nothing
         (plantask sheep (paramappend sheeps %!taskinstanceid)
                         (queuewhen(allsuccess %sheeps)))
             # will be queue as soon as third is canceled because at this
             # point the condition can no longer be met
         (plantask sheep (paramappend sheeps %!taskinstanceid)
                         (queuewhen(allfinished %sheeps)))
             # will stay planned forever because he waits for himself
             # since at evaluation time %sheeps evaluates to a list of
             # all sheeps ids, including this one
         (requesttask wolf(lone))
             # plantask can be mixted with requesttask, even if lone (out of
             # herd)
       )
     )
  ```
  supported condition operators are:
  "allfinished" "anyfinished" "allsuccess" "allfailure" "allcanceled"
  "nosuccess" "nofailure" "nocanceled" "anysuccess" "anyfailure" "anycanceled"
  "anynonsuccess" "anynonfailure" "anynoncanceled" "allfinishedanysuccess"
  "allfinishedanyfailure" "allfinishedanycanceled" "isempty" "isnotempty"
  "true" "false"
  %-evaluation is done in the herder task context, hence making it possible to
  test a dynamically appended list of tasks ids thanks to paramappend
- taskinstance field 3 Request date is renamed to Creation date
- new taskinstance fields: 15 Queue date, 16 Time planned, 17 Queue condition,
  18 Cancel condition
- wui: columns of the unfinished task instances view (displayed in the middle
  of overview page) has been changed to be more convenient with new
  planned status and recently introduced herd mechanism and waiting status
- config: instanceparam new config element on task, tasktemplate, taskgroup
  and global level: set an instance param of the same name at task
  instantiation time, evaluating its value only once
  example (task foo (instanceparam dice "%{=random:6:1}")) will roll the dice
  only once at task instantiation and keep the value, whereas (param dice
  "%{=random:6:1}") will roll the dice each time %dice is evaluated
  %-evaluation is done in the context of the task instance
- new tasks/tasktemplates/taskgroups field: 22 Instance params
- herdingpolicy now defaults to allsuccess rather then nofailure
  hence any canceled child task will be enough to make the herder task fails

Minor improvements
- action: log action now includes task params in its evaluation context
- wui: display task and taskgroup instance params on task page
- wui: display applied templates on task page
- %-evaluation of overriding params (params passed to a task instance at
  instantiation time, being it via API or trigger or requesttask action)
  is now done in full instance context, including parameters, previously
  only its pseudoparameters (e.g. %!taskid) were available, this is
  consistant with instanceparam %-evaluation context
- wui: new icons for waiting (checkered flag) and planned (calendar) status,
  new color (green) for planned status. waiting keeps its current color (same
  blue than running, which is consistent, and easier to see now with the icon)

Behind-the-curtain improvements
- switching qrond and all libs back to c++17 even if c++20 is available

# From 1.11.1 to 1.11.2 (2022-01-10)
Bugfixes
- herds: onfinish/onsuccess/onfailure requesttask actions were not triggered
  for herded tasks
- alert: gridboards were no longer updated
- wui: "Herded task instances" was not updated in herds view when a new
  herded task is requested when the herder is waiting

# From 1.11.0 to 1.11.1 (2022-01-10)
New features and notable changes
- herds: include onfinish/onsuccess/onfailure requesttask children in herd
  and wait for them

Bugfixes
- herds: also wait for herded tasks that began after begining to wait

# From 1.10.1 to 1.11.0 (2022-01-09)
New features and notable changes
- removed workflows (workflow mean, subtasks, steps, etc.)
- introduced herds:
  - connecting tasks started through requesttask action to their ancestor
    new taskinstances fields: 10 Herd Id, 11 Herded Tasks Instances,
      14 Total time
  - herder task (the first one) of a herd waits by default for other tasks
      before finishing and its result (success) is deduced from herded tasks
      result (by default, again)
    new taskinstance status: waiting
    everything that was named "end" or "ended" is now either (mostly) "stop"
      or "stopped", or (when after waiting) "finish" or "finished"
      a task stops when the process/query/etc. is finished
      a task finishes when it has nothing left to wait for after stopping
    new task config element: herdingpolicy { allsuccess nofailure onesuccess
      ownstatus nowait }
      defaults to nofailure (waits and ends on success only if herdered tasks
      all finished in success or canceled statuses)
    new taskinstances fields: 12 Finish Date, 13 Time waiting
 - new default enqueue policy: enqueueuntilmaxinstances
    above max: now create canceled tasks rather than rejecting the request
    was: enqueueanddiscardqueued

Minor improvements
- !endate taskinstance pseudo param has been renamed to !stopdate
- requesttask action new parameter: (lone) to request a task out of the herd
- http api: requesttask (/do/v1/tasks/request/) new params:
  herdid: request a task within an existing (and not yet finished) herd
  force=true: ignore most conditions to run a queued task (maxinstances...)
- http api taskslist no longer propose (bugged) status filters but instead
  a subset of "current" task instances, that is of unfinished task instances
  plus a few very soon finished tasks instances
  /rest/v1/taskinstances/list.{csv,html} no longer supports status param
  /rest/v1/taskinstances/current/list.{csv,html} new endpoints
- http api: requesttask params validated against requestformfield format
- wui: new herds view on tasks page
- wui: Running time (field 7) was replaced with Total time (field 14) on
  existing task instances views
- when target is invalid, set task to failure rather than canceling it
- tasks trigger diagram: fixed inversed arrows on global requesttask edges
- wui/http api: no longer display comments in task config.pf
- clarifying ui title of "Request form overridable params"

Bugfixes
- sub-minute cron timers was randomly fired since Qt introduced coarse timers

Behind-the-curtain improvements
- switching qrond and all libs to c++20

# From 1.10.0 to 1.10.1 (2021-12-21)
Minor improvements
- task trigger diagram: taskrequest edges constraint layout (again)
- %-evaluation: introduced %=match

Bugfixes
- fixed 2 bugs introduced by task templates implementation
- params were sent to ssh tasks instead of vars
- triggers deduced from event subscriptions was no longer visible in
  wui and api

# From 1.9.10 to 1.10.0 (2021-12-14)
New features and notable changes
- based on Qt6, no longer supporting Qt5
- config: introducing task templates
- new tasks field: 20 Applied templates
- adding new docker execution mean
- config: taskgroups now inherit from each others
  e.g. taskgroup foo.bar contains params, event subscriptions, etc. of
  taskgroup foo (if taskgroup foo exists)

Minor improvements and bug fixes
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

Bugfixes
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

Behind-the-curtain improvements
- many code fixes due to warnings from new compilers versions introduced
  with Qt6

# From 1.9.9 to 1.9.10 (2021-11-07)
Minor improvements and bug fixes
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

Bugfixes
- rest api: fixed /rest/v1/resources/*.csv which responded almost garbage
- fixed %-evaluation infinite loop when param foo is defined as %foo

Behind-the-curtain improvements
- source-level compatibility with Qt 6, therefore sticking to Qt 5.15 (last Qt 5 LTS)
- gitlab-ci: added artifacts

# From 1.9.8 to 1.9.9 (2017-04-24)
Minor improvements and bug fixes
- wui: don't elide command on task detail page
- api: disabling pagination on every csv endpoint
- doc: misc fixes

Behind-the-curtain improvements
- portability: backward compatibility down to Qt 5.3 again (Debian 8)
- more consistant build procedures across platforms
- wui/api: migrating several endpoints from textview to on-the-fly formatting
    /rest/v1/{tasks,taskgroups,steps}/list.csv
    /rest/v1/alerts_{subscriptions,settings}/list.csv
    /rest/v1/{gridboards,calendars}/list.csv
    /rest/v1/logs/logfiles.csv
- library libqtssu has been renamed to libp6core

# From 1.9.7 to 1.9.8 (2017-01-18)
Minor improvements and bug fixes
- wui: fixed a regression in 1.9.7 where static urls needed auth again

# From 1.9.6 to 1.9.7 (2017-01-18)
New features and notable changes
- alerts: visibilitywindow now applies to one-shot alerts in addition to
  stateful ones
- alerts: introducing acceptabilitywindow to avoid even more nightly alerts
  sample config:
  ```(alerts(settings(pattern myalert)(acceptabilitywindow * * 0-4 * * *)))```

Minor improvements
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

Behind-the-curtain improvements
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
Minor improvements and fixes
- wui: added /do/v1/tasks/abort_instances/%taskid
  /do/v1/tasks/cancel_requests/%taskid and
  /do/v1/tasks/cancel_requests_and_abort_instances/%taskid
  api endpoints
- wui: added version number on overview and about pages
- dependencies upgrade (libqtpf, libbqtssu)

# From 1.9.4 to 1.9.5 (2016-09-12)
Bugfixes
- fixed enqueueall/enqueueanddiscardqueued queuing policies inversion
  version 1.9.3 did mess up the two policies, and moreover introduce a
  regression in default policy

Minor improvements
- wui: more user manual links from task details page
- wui: added queueing policy to task detail page
- wui: fixed taskinstance queued status label

# From 1.9.3 to 1.9.4 (2016-09-08)
Bugfixes
- fixed regression automatic taskinstance date params

# From 1.9.2 to 1.9.3 (2016-09-07)
New features and notable changes
- scheduler: introducing task enqueuepolicy parameter, which replaces and
  enhance former discardaliasesonstart
  see http://qron.eu/doc/master/user-manual.html#enqueuepolicy
- alerts: introducing visibilitywindow to e.g. avoid nightly alerts
  sample config:
  ```(alerts(settings(pattern myalert)(visibilitywindow * * 8-22 * * *)))```

Minor improvements and fixes
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
Bugfixes
- restored /rest/do?event=requestTask&taskid= web api entry point

# From 1.9.0 to 1.9.1 (2016-07-18)
New features and notable changes
- fixed severe bug in ssh execution mean command line arguments spliting
  that disallowed having a full multi-line shell script in config file
- introduced a new task param "command.shell" that make it possible to
  choose another execution shell than the default one for ssh and local
  means
- fully rewritten request form field's 'allowedvalues' to use csv value
  list that preserves config file values order, enables setting a display
  label different from the value; allowedvalues can also be defined in
  a csv file or network resource (such as http://myapp/myvalues/)

Minor improvements and fixes
- request form field's 'allowedvalues' now defaults to "" when no default
  value is set
- fixed a rather rare case where mayrise alerts were raised, hence
  improving signal/noise ratio (when both mayrise and rise delays where
  reached during the same asynchronous state check loop, the alerts was
  always raised, now it's raised only if rise delay was reached before
  mayrise delay)
- fixed linux packaging where libQt5Sql.so.5 was lacking

# From 1.8.5 to 1.9.0 (2016-07-08)
New features and notable changes
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

Minor improvements and fixes
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

Behind-the-curtain improvements
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

# From 1.8.4 to 1.8.5 (2015-9-15)
- Few maintenance minor changes and bugfixes
  note: 1.8.4 and 1.8.5 were maintenance releases done in parallel of 1.9.0
  development

# From 1.8.3 to 1.8.4 (2015-7-26)
- Few maintenance minor changes and bugfixes
  note: 1.8.4 and 1.8.5 were maintenance releases done in parallel of 1.9.0
  development

# From 1.8.2 to 1.8.3 (2015-7-19)
- Few maintenance minor changes and bugfixes

# From 1.8.1 to 1.8.2 (2015-7-15)

# From 1.8.0 to 1.8.1 (2015-7-3)

# From 1.7.2 to 1.8.0 (2015-7-1)

# From to 1.7.2  (2014-11-23)

# From to 1.7.1  (2014-11-13)

# From to 1.7.0  (2014-11-2)

# From to 1.6.1  (2014-1-19)

# From to 1.6.0  (2014-1-9)

# From to 1.5.7 (2013-11-15)
- Few maintenance minor changes and bugfixes
  note: 1.5.7 was a maintenance release done in parallel of 1.6.0
  development

# From to 1.5.6 (2013-9-4)

