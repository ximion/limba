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
#include "listaller.h"

#include "li-config-data.h"
#include "li-file-list.h"

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

	g_object_unref (cdata);
}

void
test_filelist ()
{
	LiFileList *flist;
	gchar *fname;
	GList *files;
	GList *l;
	const gchar *cstr;
	gboolean ret;

	g_debug ("File list read test");

	/* create new file list with hashes */
	flist = li_file_list_new (TRUE);

	/* open */
	fname = g_build_filename (datadir, "test-files.list", NULL);
	ret = li_file_list_open_file (flist, fname);
	g_free (fname);
	g_assert (ret);

	files = li_file_list_get_files (flist);
	g_assert (g_list_length (files) == 8);
	for (l = files; l != NULL; l = l->next) {
		_cleanup_free_ gchar *str;
		LiFileEntry *fe = (LiFileEntry*) l->data;

		if (g_strcmp0 (li_file_entry_get_fname (fe), "libvorbis.so.0") == 0) {
			cstr = li_file_entry_get_destination (fe);
			g_assert (g_strcmp0 (cstr, "%INST%/libs64") == 0);
			cstr = li_file_entry_get_hash (fe);
			g_assert (g_strcmp0 (cstr, "9abdb152eed431cf205917c778e80d398ef9406201d0467fbf70a68c21e2a6ff") == 0);
		} else if (g_strcmp0 (li_file_entry_get_fname (fe), "name with spaces.txt") == 0) {
			cstr = li_file_entry_get_destination (fe);
			g_assert (g_strcmp0 (cstr, "%INST%/libs64") == 0);
			cstr = li_file_entry_get_hash (fe);
			g_assert (g_strcmp0 (cstr, "86fbf88bf19ed4c44f6f6aef1c17300395d46cab175eecf28d9f306d9272e32a") == 0);
		} else if (g_strcmp0 (li_file_entry_get_fname (fe), "StartApp") == 0) {
			cstr = li_file_entry_get_destination (fe);
			g_assert (g_strcmp0 (cstr, "%INST%") == 0);
			cstr = li_file_entry_get_hash (fe);
			g_assert (g_strcmp0 (cstr, "0ce781271b68e2c97b77e750ba899ff7d6cb64e9fdbd3d635c2696edf51af8e7") == 0);
		}

		str = li_file_entry_to_string (fe);
		g_debug ("%s", str);
	}
	g_list_free (files);
	g_object_unref (flist);

	/* ********************************** */
	g_debug ("File list write test");

	/* create new file list with hashes */
	flist = li_file_list_new (TRUE);

	fname = g_build_filename (datadir, "doap.doap", NULL);
	li_file_list_add_file (flist, fname, "%INST%/test");
	g_free (fname);

	fname = g_build_filename (datadir, "xfile1.bin", NULL);
	li_file_list_add_file (flist, fname, "%INST%");
	g_free (fname);

	fname = g_build_filename (datadir, "test-files.list", NULL);
	li_file_list_add_file (flist, fname, "%INST%/test");
	g_free (fname);

	fname = g_build_filename (datadir, "appstream.appdata.xml", NULL);
	li_file_list_add_file (flist, fname, "%INST%");
	g_free (fname);

	files = li_file_list_get_files (flist);
	g_assert (g_list_length (files) == 4);
	for (l = files; l != NULL; l = l->next) {
		_cleanup_free_ gchar *str;
		LiFileEntry *fe = (LiFileEntry*) l->data;

		if (g_strcmp0 (li_file_entry_get_fname (fe), "doap.doap") == 0) {
			cstr = li_file_entry_get_destination (fe);
			g_assert (g_strcmp0 (cstr, "%INST%/test") == 0);
			cstr = li_file_entry_get_hash (fe);
			g_assert (g_strcmp0 (cstr, "d44259c22a3878a62b2963984e4c749a959dd42fe16f83f60f1841ac4d2fa617") == 0);
		}

		str = li_file_entry_to_string (fe);
		g_debug ("%s", str);
	}
	g_list_free (files);

	ret = li_file_list_save_to_file (flist, "/tmp/test.list");
	//! g_assert (ret);

	g_object_unref (flist);
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

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* critical, error and warnings are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/Listaller/ConfigData", test_configdata);
	g_test_add_func ("/Listaller/FileList", test_filelist);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
