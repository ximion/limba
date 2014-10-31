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
test_package_read ()
{
	LiIPKPackage *ipk;
	gchar *fname;
	GError *error = NULL;

	fname = g_build_filename (datadir, "foo.ipk", NULL);
	ipk = li_ipk_package_new ();

	li_ipk_package_open_file (ipk, fname, &error);
	g_assert_no_error (error);

	li_ipk_package_install (ipk, &error);
	g_assert_no_error (error);

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

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	g_test_init (&argc, &argv, NULL);

	/* critical, error and warnings are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/Limba/IPKRead", test_package_read);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
