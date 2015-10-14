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
#include "limba.h"
#include <stdlib.h>

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
test_installer_simple ()
{
	LiInstaller *inst;
	g_autofree gchar *fname_app = NULL;
	g_autofree gchar *fname_lib = NULL;
	GError *error = NULL;

	fname_app = g_build_filename (datadir, "foobar.ipk", NULL);
	fname_lib = g_build_filename (datadir, "libfoo.ipk", NULL);

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
}

void
test_installer_embeddedpkg ()
{
	LiInstaller *inst;
	GError *error = NULL;
	g_autofree gchar *fname_full = NULL;

	fname_full = g_build_filename (datadir, "FooBar-1.0_full.ipk", NULL);

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
	GList *pkgs;
	GError *error = NULL;
	LiManager *mgr;

	mgr = li_manager_new ();

	pkgs = li_manager_get_software_list (mgr, &error);
	g_assert_no_error (error);
	g_assert (pkgs != NULL);

	g_list_free (pkgs);

	g_object_unref (mgr);
}

void
test_install_remove ()
{
	/* firs test installer */
	test_installer_simple ();

	/* now remove all software again */
	test_remove_software ();

	/* test installation of embedded package copy */
	test_installer_embeddedpkg ();

	/* test with manager if we can read required data */
	test_manager ();

	/* uninstall once again */
	test_remove_software ();
}

void
test_repository ()
{
	g_autofree gchar *rdir;
	g_autofree gchar *fname_app = NULL;
	g_autofree gchar *fname_lib = NULL;
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
test_install_from_repo ()
{
	LiInstaller *inst;
	GError *error = NULL;

	/* test an installation which fetches stuff from a remote location */
	inst = li_installer_new ();
	li_installer_open_remote (inst, "foobar-1.0", &error);
	g_assert_no_error (error);

	li_installer_install (inst, &error);
	g_assert_no_error (error);
	g_object_unref (inst);
}

void
test_pkg_cache_setup ()
{
	LiPkgCache *cache;
	LiManager *mgr;
	GError *error = NULL;

	/* write sample repository file */
	g_mkdir_with_parents ("/etc/limba/", 0755);
	g_file_set_contents ("/etc/limba/sources.list", "# Limba Unit Tests\n\n# Test Repo\nhttp://people.freedesktop.org/~mak/stuff/limba-repo/\n", -1, &error);
	g_assert_no_error (error);

	/* we need to trust the sample repository key */
	mgr = li_manager_new ();
	li_manager_receive_key (mgr, "D33A3F0CA16B0ACC51A60738494C8A5FBF4DECEB", &error);
	g_assert_no_error (error);
	g_object_unref (mgr);

	/* run a cache update */
	cache = li_pkg_cache_new ();

	li_pkg_cache_update (cache, &error);
	g_assert_no_error (error);

	g_object_unref (cache);
}

void
test_pkg_cache ()
{
	/* set up package cache */
	test_pkg_cache_setup ();

	/* try to install something from the repository */
	test_install_from_repo ();
}

int
main (int argc, char **argv)
{
	int ret;
	gchar *tmp;
	gchar *cmd;

	if (argc == 0) {
		g_error ("No test data directory specified!");
		return 1;
	}

	datadir = argv[1];
	g_assert (datadir != NULL);
	datadir = g_build_filename (datadir, "data", NULL);
	g_assert (g_file_test (datadir, G_FILE_TEST_EXISTS) != FALSE);

	/* set fake GPG home */
	tmp = g_build_filename (argv[1], "gpg", NULL);
	cmd = g_strdup_printf ("cp -r '%s' /tmp", tmp);
	system (cmd); /* meh for call to system() - but okay for the testsuite */
	g_free (tmp);
	g_free (cmd);
	g_setenv ("GNUPGHOME", "/tmp/gpg", 1);

	li_set_verbose_mode (TRUE);
	g_test_init (&argc, &argv, NULL);

	/* critical, error and warnings are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/Limba/InstallRemove", test_install_remove);
	g_test_add_func ("/Limba/Repository", test_repository);
	g_test_add_func ("/Limba/PackageCache", test_pkg_cache);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
