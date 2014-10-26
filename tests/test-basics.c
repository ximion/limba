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

#include "li-config-data.h"

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

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
