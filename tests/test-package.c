/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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
#include <stdlib.h>
#include "limba.h"

#include "li-utils-private.h"

static gchar *datadir = NULL;

void
test_compile_foobar ()
{
	g_autofree gchar *dirname = NULL;
	g_autofree gchar *inst_dir_all = NULL;
	g_autofree gchar *inst_dir_app = NULL;
	g_autofree gchar *inst_dir_lib = NULL;
	gchar *cmd;
	gint ret;

	/* TODO: This is duty for the appcompile helper... */

	dirname = g_build_filename (datadir, "..", "foobar", NULL);
	inst_dir_all = g_build_filename (dirname, "lipkg", "target", NULL);
	inst_dir_app = g_build_filename (dirname, "foo", "lipkg", "target", NULL);
	inst_dir_lib = g_build_filename (dirname, "libfoo", "lipkg", "target", NULL);
	chdir (dirname);

	if (g_file_test ("Makefile", G_FILE_TEST_EXISTS))
		system ("make distclean");

	ret = system ("./autogen.sh --prefix=/opt/bundle");
	g_assert (ret == 0);

	ret = system ("make");
	g_assert (ret == 0);

	cmd = g_strdup_printf ("make install DESTDIR=%s", inst_dir_all);
	ret = system (cmd);
	g_free (cmd);
	g_assert (ret == 0);

	/* now install the individual components of this software */

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
	g_autofree gchar *foo_dirname = NULL;
	g_autofree gchar *libfoo_dirname;
	gchar *pkgname;
	g_autofree gchar *repo_path;
	gchar *tmp;
	GError *error = NULL;

	test_compile_foobar ();

	builder = li_pkg_builder_new ();
	/* don't sign packages for now, this feature needs some more work */
	li_pkg_builder_set_sign_package (builder, FALSE);

	/* ****** */
	/* build application package */
	foo_dirname = g_build_filename (datadir, "..", "foobar", "foo", "lipkg", NULL);
	repo_path = g_build_filename (foo_dirname, "repo", NULL);
	/* ensure we don't accidentially embed a package */
	li_delete_dir_recursive (repo_path);

	pkgname = g_build_filename (datadir, "foobar.ipk", NULL);

	li_pkg_builder_create_package_from_dir (builder, foo_dirname, pkgname, &error);
	g_assert_no_error (error);

	g_free (pkgname);

	/* ****** */
	/* build library package */
	libfoo_dirname = g_build_filename (datadir, "..", "foobar", "libfoo", "lipkg", NULL);
	pkgname = g_build_filename (datadir, "libfoo.ipk", NULL);

	li_pkg_builder_create_package_from_dir (builder, libfoo_dirname, pkgname, &error);
	g_assert_no_error (error);

	/* ****** */
	/* test embedded packages, by copying libfoo */
	g_mkdir_with_parents (repo_path, 0775);
	tmp = g_build_filename (repo_path, "libfoo.ipk", NULL);
	li_copy_file (pkgname, tmp, &error);
	g_assert_no_error (error);
	g_free (pkgname);

	pkgname = g_build_filename (datadir, "FooBar-1.0_full.ipk", NULL);

	li_pkg_builder_create_package_from_dir (builder, foo_dirname, pkgname, &error);
	g_assert_no_error (error);

	g_free (pkgname);

	g_object_unref (builder);

	/* cleanup */
	li_delete_dir_recursive (repo_path);
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

	li_set_verbose_mode (TRUE);
	g_test_init (&argc, &argv, NULL);

	/* critical, error and warnings are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/Limba/IPKBuild", test_package_build);
	g_test_add_func ("/Limba/IPKRead", test_package_read);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
