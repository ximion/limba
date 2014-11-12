bin_PROGRAMS = foo

INCLUDES = \
	-I../libfoo\
	-DFOOBAR_VERSION=\"@VERSION@\"		\
	-DBINDIR=\"$(bindir)\"			\
	-DSBINDIR=\"$(sbindir)\"		\
	-DDATADIR=\"$(datadir)\"		\
	-DPREFIX=\""$(prefix)"\"		\
	-DSYSCONFDIR=\""$(sysconfdir)"\"

foo_SOURCES = main.c
foo_LDADD = -L../libfoo -lfoo -lpthread

sysconf_DATA = foo-config

desktopdir=$(datadir)/applications
desktop_DATA=foobar.desktop

EXTRA_DIST = $(sysconf_DATA) foobar.desktop
