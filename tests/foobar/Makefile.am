AUTOMAKE_OPTIONS = 1.7

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS=libfoo foo doc pkginstall

clean-local:
	-rm -rf pkginstall/inst_target
	-rm -rf foo/pkginstall/inst_target
	-rm -rf libfoo/pkginstall/inst_target
