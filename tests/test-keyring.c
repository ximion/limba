/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#include "li-keyring.h"
#include "li-utils-private.h"

static gchar *datadir = NULL;

void
test_keyring () {
	LiKeyring *kr;
	GError *error = NULL;

	kr = li_keyring_new ();

	li_keyring_import_key (kr, "D33A3F0CA16B0ACC51A60738494C8A5FBF4DECEB", LI_KEYRING_KIND_USER, &error);
	g_assert_no_error (error);

	g_object_unref (kr);
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

	g_test_add_func ("/Limba/Keyring", test_keyring);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
