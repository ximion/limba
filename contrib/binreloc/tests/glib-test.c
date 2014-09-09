#include <glib.h>
#include <stdlib.h>
#include "glib-binreloc.h"
#include "lib.h"


int
main ()
{
	GError *error = NULL;
	gchar *exe, *exedir, *prefix, *bin, *sbin, *data, *locale, *lib, *libexec, *etc, *shared;

	if (!gbr_init (&error)) {
		g_print ("BinReloc failed to initialize:\n");
		g_print ("Domain: %d (%s)\n",
			 (int) error->domain,
			 g_quark_to_string (error->domain));
		g_print ("Code: %d\n", error->code);
		g_print ("Message: %s\n", error->message);
		g_error_free (error);
		g_print ("----------------\n");
	}

	exe     = gbr_find_exe         ("default exe name");
	exedir  = gbr_find_exe_dir     ("default exe dir name");
	prefix  = gbr_find_prefix      ("default prefix dir");
	bin     = gbr_find_bin_dir     ("default bin dir");
	sbin    = gbr_find_sbin_dir    ("default sbin dir");
	data    = gbr_find_data_dir    ("default data dir");
	locale  = gbr_find_locale_dir  ("default locale dir");
	lib     = gbr_find_lib_dir     ("default lib dir");
	libexec = gbr_find_libexec_dir ("default libexec dir");
	etc     = gbr_find_etc_dir     ("default etc dir");
	shared  = locate_libshared     ();

	g_print ("Executable filename : %s\n", exe);
	g_print ("Executable directory: %s\n", exedir);
	g_print ("Prefix     : %s\n", prefix);
	g_print ("Bin dir    : %s\n", bin);
	g_print ("Sbin dir   : %s\n", sbin);
	g_print ("Data dir   : %s\n", data);
	g_print ("Locale dir : %s\n", locale);
	g_print ("Library dir: %s\n", lib);
	g_print ("Libexec dir: %s\n", libexec);
	g_print ("Etc dir    : %s\n", etc);
	g_print ("libshared  : %s\n", shared);

	g_free (exe);
	g_free (exedir);
	g_free (prefix);
	g_free (bin);
	g_free (sbin);
	g_free (data);
	g_free (locale);
	g_free (lib);
	g_free (libexec);
	g_free (etc);
	if (shared != NULL)
		free (shared);
	return 0;
}
