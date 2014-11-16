/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include "limba.h"
#include <stdlib.h>

#include "li-utils-private.h"

static gchar *datadir = NULL;

void
test_compile_foobar ()
{
	_cleanup_free_ gchar *dirname = NULL;
	_cleanup_free_ gchar *inst_dir_all = NULL;
	_cleanup_free_ gchar *inst_dir_app = NULL;
	_cleanup_free_ gchar *inst_dir_lib = NULL;
	gchar *cmd;
	gint ret;

	/* TODO: This is duty for the appcompile helper... */

	dirname = g_build_filename (datadir, "..", "foobar", NULL);
	inst_dir_all = g_build_filename (dirname, "pkginstall", "inst_target", NULL);
	inst_dir_app = g_build_filename (dirname, "foo", "pkginstall", "inst_target", NULL);
	inst_dir_lib = g_build_filename (dirname, "libfoo", "pkginstall", "inst_target", NULL);
	chdir (dirname);

	ret = system ("./autogen.sh --prefix=/opt/swroot");
	g_assert (ret == 0);

	ret = system ("make");
	g_assert (ret == 0);

	cmd = g_strdup_printf ("make install DESTDIR=%s", inst_dir_all);
	ret = system (cmd);
	g_free (cmd);
	g_assert (ret == 0);

	/* no install the individual components of this software */

	/* app */
	cmd = g_build_filename (dirname, "foo", NULL);
	chdir (cmd);
	g_free (cmd);

	cmd = g_strdup_printf ("make install DESTDIR=%s", inst_dir_app);
	ret = system (cmd);
	g_free (cmd);
	g_assert (ret == 0);

	/* lib */
	cmd = g_build_filename (dirname, "libfoo", NULL);
	chdir (cmd);
	g_free (cmd);

	cmd = g_strdup_printf ("make install DESTDIR=%s", inst_dir_lib);
	ret = system (cmd);
	g_free (cmd);
	g_assert (ret == 0);

	chdir ("/tmp");
}

void
test_package_build ()
{
	LiPkgBuilder *builder;
	gchar *dirname;
	gchar *pkgname;
	GError *error = NULL;

	test_compile_foobar ();

	builder = li_pkg_builder_new ();

	/* build application package */
	dirname = g_build_filename (datadir, "..", "foobar", "foo", "pkginstall", NULL);
	pkgname = g_build_filename (datadir, "foobar.ipk", NULL);

	li_pkg_builder_create_package_from_dir (builder, dirname, pkgname, &error);
	g_assert_no_error (error);

	g_free (dirname);
	g_free (pkgname);

	/* build library package */
	dirname = g_build_filename (datadir, "..", "foobar", "libfoo", "pkginstall", NULL);
	pkgname = g_build_filename (datadir, "libfoo.ipk", NULL);

	li_pkg_builder_create_package_from_dir (builder, dirname, pkgname, &error);
	g_assert_no_error (error);

	g_free (dirname);
	g_free (pkgname);

	g_object_unref (builder);
}

void
test_package_read ()
{
	LiPackage *ipk;
	gchar *fname;
	GError *error = NULL;

	fname = g_build_filename (datadir, "libfoo.ipk", NULL);
	ipk = li_package_new ();

	g_assert (li_package_get_id (ipk) == NULL);

	li_package_open_file (ipk, fname, &error);
	g_assert_no_error (error);

	g_assert (g_strcmp0 (li_package_get_id (ipk), "libfoo-1.0") == 0);

	g_object_unref (ipk);
}

int
main (int argc, char **argv)
{
	int ret;

	if (argc == 0) {
		g_error ("No test data directory specified!");
		return 1;
	}

	datadir = argv[1];
	g_assert (datadir != NULL);
	datadir = g_build_filename (datadir, "data", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	li_set_unittestmode (TRUE);

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* critical, error and warnings are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/Limba/IPKBuild", test_package_build);
	g_test_add_func ("/Limba/IPKRead", test_package_read);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
