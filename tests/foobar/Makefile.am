AUTOMAKE_OPTIONS = 1.7

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS=libfoo foo doc pkginstall

desktopdir=$(datadir)/applications
desktop_DATA=foobar.desktop

EXTRA_DIST=foobar.desktop

clean-local:
	-rm -rf pkginstall/inst_target
	-rm -rf foo/pkginstall/inst_target
	-rm -rf libfoo/pkginstall/inst_target
