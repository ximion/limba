# PyReloc - a library for creating relocatable python applications
# Written by: Jan Niklas Hasse <jhasse@gmail.com>
# http://autopackage.org/
#
# This source code is public domain. You can relicense this code
# under whatever license you want.
#
# See http://autopackage.org/docs/pyreloc/ for
# more information and how to use this.

import os

EXE = __file__
EXEDIR = os.path.dirname(EXE)
PREFIX = os.path.normpath(os.path.join(EXEDIR, ".."))

BINDIR = os.path.join(PREFIX, "bin")
SBINDIR = os.path.join(PREFIX, "sbin")
DATADIR = os.path.join(PREFIX, "share")
LIBDIR = os.path.join(PREFIX, "lib")
LIBEXECDIR = os.path.join(PREFIX, "libexec")
ETCDIR = os.path.join(PREFIX, "etc")
BINDIR = os.path.join(PREFIX, "bin")

LOCALE = os.path.join(DATADIR, "locale")
