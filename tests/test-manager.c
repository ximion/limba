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

#include "li-pkg-cache.h"
#include "li-utils-private.h"

static gchar *datadir = NULL;

void
test_remove_software ()
{
	LiManager *mgr;
	GError *error = NULL;

	mgr = li_manager_new ();

	li_manager_remove_software (mgr, "libfoo-1.0", &error);
	g_assert_error (error, LI_MANAGER_ERROR, LI_MANAGER_ERROR_DEPENDENCY);
	g_error_free (error);
	error = NULL;

	li_manager_remove_software (mgr, "foobar-1.0", &error);
	g_assert_no_error (error);

	li_manager_remove_software (mgr, "libfoo-1.0", &error);
	g_assert_no_error (error);

	g_object_unref (mgr);
}

void
test_installer ()
{
	LiInstaller *inst;
	_cleanup_free_ gchar *fname_app = NULL;
	_cleanup_free_ gchar *fname_lib = NULL;
	_cleanup_free_ gchar *fname_full = NULL;
	GError *error = NULL;

	fname_app = g_build_filename (datadir, "foobar.ipk", NULL);
	fname_lib = g_build_filename (datadir, "libfoo.ipk", NULL);
	fname_full = g_build_filename (datadir, "FooBar-1.0_full.ipk", NULL);

	inst = li_installer_new ();

	li_installer_open_file (inst, fname_app, &error);
	g_assert_no_error (error);
	li_installer_install (inst, &error);
	/* this has to fail, we don't have libfoo yet */
	g_assert_error (error, LI_INSTALLER_ERROR, LI_INSTALLER_ERROR_DEPENDENCY_NOT_FOUND);
	g_error_free (error);
	error = NULL;

	/* install library */
	li_installer_open_file (inst, fname_lib, &error);
	g_assert_no_error (error);
	li_installer_install (inst, &error);
	g_assert_no_error (error);

	/* now we should be able to install the app */
	li_installer_open_file (inst, fname_app, &error);
	g_assert_no_error (error);
	li_installer_install (inst, &error);
	g_assert_no_error (error);

	g_object_unref (inst);

	/* now remove all software again */
	test_remove_software ();

	/* test the installation of a package with embedded dependencies */
	inst = li_installer_new ();
	li_installer_open_file (inst, fname_full, &error);
	g_assert_no_error (error);
	li_installer_install (inst, &error);
	g_assert_no_error (error);
	g_object_unref (inst);
}

void
test_manager ()
{
	GPtrArray *pkgs;
	LiManager *mgr;

	mgr = li_manager_new ();

	pkgs = li_manager_get_installed_software (mgr);
	g_assert (pkgs != NULL);

	g_ptr_array_unref (pkgs);

	g_object_unref (mgr);
}

void
test_repository ()
{
	_cleanup_free_ gchar *rdir;
	_cleanup_free_ gchar *fname_app = NULL;
	_cleanup_free_ gchar *fname_lib = NULL;
	LiRepository *repo;
	GError *error = NULL;

	rdir = li_utils_get_tmp_dir ("repo");
	repo = li_repository_new ();

	li_repository_open (repo, rdir, &error);
	g_assert_no_error (error);

	/* add packages */
	fname_app = g_build_filename (datadir, "foobar.ipk", NULL);
	li_repository_add_package (repo, fname_app, &error);
	g_assert_no_error (error);

	fname_lib = g_build_filename (datadir, "libfoo.ipk", NULL);
	li_repository_add_package (repo, fname_lib, &error);
	g_assert_no_error (error);

	li_repository_save (repo, &error);
	g_assert_no_error (error);

	li_delete_dir_recursive (rdir);
	g_object_unref (repo);
}

void
test_pkg_cache ()
{
	LiPkgCache *cache;
	GError *error = NULL;

	cache = li_pkg_cache_new ();

	li_pkg_cache_update (cache, &error);
	g_assert_no_error (error);

	g_object_unref (cache);
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

	li_set_verbose_mode (TRUE);
	g_test_init (&argc, &argv, NULL);

	/* clean up test directory */
	li_delete_dir_recursive ("/var/tmp/limba/test-root");

	/* critical, error and warnings are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/Limba/InstallRemove", test_installer);
	g_test_add_func ("/Limba/Manager", test_manager);
	g_test_add_func ("/Limba/Repository", test_repository);
	g_test_add_func ("/Limba/PackageCache", test_pkg_cache);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
