bin_PROGRAMS = foo

INCLUDES = \
	-I../libfoo\
	-DFOOBAR_VERSION=\"@VERSION@\"

foo_SOURCES = main.c prefix.c
foo_LDADD = -L../libfoo -lfoo -lpthread

sysconf_DATA = foo-config

EXTRA_DIST = $(sysconf_DATA)
