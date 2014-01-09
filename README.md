Qron free scheduler
===================

ABOUT
-----

Qron is a free software enterprise scheduler.
See http://qron.eu/ for more information about this software and
for online documentation.

LICENSE
-------

This software is made availlable to you under the terms of the GNU Affero
General Public License version 3, see LICENSE file content.

BUILD INSTRUCTIONS
------------------

This program requires Qt >= 5.2.0 to be built.
Actually Qt 5.0.0 should be enough for building, however there is a bug in Qt
until 5.2.0 that can lead to random crashes in qron when executing tasks
through http, therefore we advise to run with Qt 5.2.0 libs or above.

The git repository uses git submodules to handle libqtpf and libqtssu
dependencies, therefore you should use following steps to clone repository
and submodules repositories:

``` bash
git clone git://github.com/g76r/qron.git
cd qron
git submodule init
git submodule update
```

Then one should build dependencies before qron.
This can be done transparently using the top directory qmake project or any
external program such as an IDE like Qt Creator.
For instance, these commands work on linux provided a Qt 5 devkit is installed
and in your PATH:

``` bash
qmake
make
```

CONTRIBUTIONS
-------------

We do not yet accept code contributions, but we accept bug reports and feature
requests. Please feel free to fill in a github's issue (here: https://github.com/g76r/qron/issues) or to contact us by mail (see contact page on our web site:
http://qron.eu/).

