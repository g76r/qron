Qron free scheduler
===================

ABOUT
-----

Qron is a free software enterprise scheduler.
See http://www.qron.eu/ for more information about this software and
for online documentation.

LICENSE
-------

This software is made availlable to you under the terms of the GNU Affero
General Public License version 3, see LICENSE file content.

BUILD INSTRUCTIONS
------------------

This program requires Qt >= 4.7 to be built.

The git repository uses git submodules to handle libqtpf dependency, therefore
you should use following steps to clone repository and submodules repositories:

``` bash
git clone git://github.com/g76r/qron.git
cd qron
git submodule init
git submodule update
```

Then one should build libqtpf before qron since it's a dependency.
This can be done with the top directory qmake project or with any external
program such as an IDE like Qt Creator.
For instance, these commands work on most modern linux distros:

``` bash
qmake
make
```

On some old distros that still use Qt 3 as the default, you may need to type:

``` bash
qmake-qt4
make
```

