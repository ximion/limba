bin_PROGRAMS = foo

INCLUDES = \
	-I../libfoo\
	-DFOOBAR_VERSION=\"@VERSION@\"

foo_SOURCES = main.c prefix.c
foo_LDADD = -L../libfoo -lfoo -lpthread

sysconf_DATA = foo-config

desktopdir=$(datadir)/applications
desktop_DATA=foobar.desktop

EXTRA_DIST = $(sysconf_DATA) foobar.desktop
