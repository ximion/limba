#include <stdio.h>
#include <stdlib.h>
#include "normal-binreloc.h"
#include "lib.h"

#define my_free(x) if (x != (char *) 0) free (x)


int
main ()
{
	BrInitError error;
	char *exe, *exedir, *prefix, *bin, *sbin, *data, *locale, *lib, *libexec, *etc, *shared;

	if (!br_init (&error))
		printf ("*** BinReloc failed to initialize. Error: %d\n", error);

	exe     = br_find_exe         ("default exe name");
	exedir  = br_find_exe_dir     ("default exe dir name");
	prefix  = br_find_prefix      ("default prefix");
	bin     = br_find_bin_dir     ("default bin dir");
	sbin    = br_find_sbin_dir    ("default sbin dir");
	data    = br_find_data_dir    ("default data dir");
	locale  = br_find_locale_dir  ("default locale dir");
	lib     = br_find_lib_dir     ("default lib dir");
	libexec = br_find_libexec_dir ("default libexec dir");
	etc     = br_find_etc_dir     ("default etc dir");
	shared  = locate_libshared    ();

	printf ("Executable filename : %s\n", exe);
	printf ("Executable directory: %s\n", exedir);
	printf ("Prefix     : %s\n", prefix);
	printf ("Bin dir    : %s\n", bin);
	printf ("Sbin dir   : %s\n", sbin);
	printf ("Data dir   : %s\n", data);
	printf ("Locale dir : %s\n", locale);
	printf ("Library dir: %s\n", lib);
	printf ("Libexec dir: %s\n", libexec);
	printf ("Etc dir    : %s\n", etc);
	printf ("libshared  : %s\n", shared);

	my_free (exe);
	my_free (exedir);
	my_free (prefix);
	my_free (bin);
	my_free (sbin);
	my_free (data);
	my_free (locale);
	my_free (lib);
	my_free (libexec);
	my_free (etc);
	my_free (shared);
	return 0;
}
