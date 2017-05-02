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
General Public License version 3, see AGPL-3.0.txt file content.

BUILD INSTRUCTIONS
------------------

This program requires at build time:
* Qt >= 5.6.0 development libs (with QtCore, QtNetwork, QtSql modules);
  most of the time, the last Qt version shipped with current stable Debian
  release (currently Qt 5.3.2 on Debian 8 Jessie) should work,
  but some minor features will lack or differ (currently quotes in logs,
  reliability of rare errors detections, etc.);
* graphviz binaries (to compile some documentation diagrams)

For instance, on Debian-based Linux distribution, these command should be
enough to install all compilation prerequisites:
``` bash
sudo apt-get install git g++ qt5-default qt5-qmake make graphviz
```

In addition, the git repository uses git submodules to handle libqtpf and
libp6core dependencies, therefore you should use following steps to clone
repository and submodules repositories:

``` bash
git clone https://gitlab.com/g76r/qron.git
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

When built on Linux, binaries can be found under "linux" directory, with a
README file explaining how to start the daemon.

CONTRIBUTIONS
-------------

We do not yet accept code contributions, but we accept bug reports and feature
requests. Please feel free to fill in a github's issue (here: https://github.com/g76r/qron/issues) or to contact us by mail (see contact page on our web site:
http://qron.eu/).

