AUTOMAKE_OPTIONS = 1.7

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS=libfoo foo doc lipkg

clean-local:
	-rm -rf lipkg/inst_target
	-rm -rf foo/lipkg/inst_target
	-rm -rf libfoo/lipkg/inst_target
