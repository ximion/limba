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

#include "li-config-data.h"
#include "li-utils-private.h"

static gchar *datadir = NULL;

void
test_configdata ()
{
	LiConfigData *cdata;
	gchar *fname;
	GFile *file;
	gboolean ret;
	gchar *str;

	fname = g_build_filename (datadir, "lidatafile.test", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);
	g_assert (g_file_query_exists (file, NULL));

	cdata = li_config_data_new ();
	li_config_data_load_file (cdata, file);
	g_object_unref (file);

	ret = li_config_data_open_block (cdata, "Section", "test1", TRUE);
	g_assert (ret);

	str = li_config_data_get_value (cdata, "Sample");
	ret = g_strcmp0 (str, "valueX") == 0;
	g_assert (ret);
	g_free (str);

	ret = li_config_data_open_block (cdata, "Section", "test2", TRUE);
	g_assert (ret);

	str = li_config_data_get_value (cdata, "Sample");
	ret = g_strcmp0 (str, "valueY") == 0;
	g_assert (ret);
	g_free (str);

	str = li_config_data_get_value (cdata, "Multiline");
	ret = g_strcmp0 (str, "A\nB\nC\nD") == 0;
	g_assert (ret);
	g_free (str);

	str = li_config_data_get_data (cdata);
	li_config_data_set_value (cdata, "Foooooo", "Baaaaaaar");
	g_debug ("%s", str);
	g_free (str);

	g_object_unref (cdata);
}

void
test_pkgindex ()
{
	LiPkgIndex *idx;
	gchar *fname;
	GFile *file;
	GPtrArray *pkgs;
	LiPkgInfo *pki;
	gchar *str;

	fname = g_build_filename (datadir, "pkg-index", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);
	g_assert (g_file_query_exists (file, NULL));

	idx = li_pkg_index_new ();
	li_pkg_index_load_file (idx, file);
	g_object_unref (file);

	pkgs = li_pkg_index_get_packages (idx);
	g_assert (pkgs->len == 3);

	pki = g_ptr_array_index (pkgs, 1);
	g_assert_cmpstr (li_pkg_info_get_name (pki), ==, "testB-1.1");
	g_assert_cmpstr (li_pkg_info_get_appname (pki), ==, "Test B");
	g_assert_cmpstr (li_pkg_info_get_version (pki), ==, "1.1");
	g_assert_cmpstr (li_pkg_info_get_checksum_sha256 (pki), ==, "31415");

	g_object_unref (idx);

	/* write */
	idx = li_pkg_index_new ();

	pki = li_pkg_info_new ();
	li_pkg_info_set_name (pki, "Test");
	li_pkg_info_set_version (pki, "1.4");
	li_pkg_index_add_package (idx, pki);
	g_object_unref (pki);

	pki = li_pkg_info_new ();
	li_pkg_info_set_name (pki, "Alpha");
	li_pkg_info_set_appname (pki, "Test-Name");
	li_pkg_info_set_version (pki, "1.8");
	li_pkg_index_add_package (idx, pki);
	g_object_unref (pki);

	str = li_pkg_index_get_data (idx);
	g_assert_cmpstr (str, ==, "Format-Version: 1.0\n\nPkgName: Test\nName: Test\nVersion: 1.4\n\nPkgName: Alpha\nName: Test-Name\nVersion: 1.8\n");
	g_free (str);

	g_object_unref (idx);
}

void
test_versions () {
	g_assert (li_compare_versions ("6", "8") == -1);
	g_assert (li_compare_versions ("0.6.12b-d", "0.6.12a") == 1);
	g_assert (li_compare_versions ("7.4", "7.4") == 0);
	g_assert (li_compare_versions ("ab.d", "ab.f") == -1);
	g_assert (li_compare_versions ("0.6.16", "0.6.14") == 1);

	g_assert (li_compare_versions ("3.0.rc2", "3.0.0") == -1);
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

	g_test_add_func ("/Limba/ConfigData", test_configdata);
	g_test_add_func ("/Limba/PackageIndex", test_pkgindex);
	g_test_add_func ("/Limba/CompareVersions", test_versions);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
