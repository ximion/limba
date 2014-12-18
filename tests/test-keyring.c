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

const gchar *sig_signature = "-----BEGIN PGP SIGNED MESSAGE-----\n"
"Hash: SHA1\n\n"
"d0bb8a23da064efb222b1c8407711f231655460c6ed643b3ff311b6f92f99d69	repo/repo-index\n"
"ab33f19095252116ef503c4b7c3562410a1f4961a27c919f7e4efbb53e7df15a	control\n"
"c34d4eb3f7ef6343998e77a32f0fb85a78201229086190dddaf00e4a0ab17731	metainfo.xml\n"
"096ba20c95d07d96d5bb8de664cc6a2dd5bcf87ad4120978e3d3babedf0a2d3b	main-data.tar.xz\n"
"-----BEGIN PGP SIGNATURE-----\n"
"Version: GnuPG v2\n\n"
"iQIcBAEBAgAGBQJUkY9rAAoJEElMil+/Tezr9PYQAIkXBX8XDjw3tTruCkTCqzS5\n"
"V05H1I0TUWIS5at4TB1GEbQBQhiwKo44dVVqJQerZEeVGeQKi7d41J9/87Kfv9KO\n"
"Dyx6NizHqTHOfqY3xV/s9Zrm/SIQFdjAA69i3dTiOJwSWHdbdvOKPgA8wrtVYl5D\n"
"mVlydTONjdjFia8tRqsgQd4X2VNTuvO8YBZkKCv4d0mYVw0BNID0ZknNr1BnWecy\n"
"xj+7BCUMWm/l3Ykzk3q68h0R/hz32/mWHRWn3Ntu9LRYKqzGsxTluXBDUi9Zb3/h\n"
"DWvGDIvV79BHUK/P8suRZns+dG/+JJlChJzW0bKJRptf9gSyJeJjjhsVkYoEuZ8Q\n"
"mEzeJjk+tl22RiWWm2WiYOUTkwOuhLzEf8kjS/3i8QWnaEZhWujBN9fF6oiOLzgg\n"
"Hm4uZ1z4hGhVLxIN2Ai/7PJsma/SMwKovlAwJdFfsvjR2xs2nD0wIWiyfYIaKIVc\n"
"rJuMCzEmh8gQLv/6imdbOJw2QP2uA4QbafLNipF9zaPNL0UsrGF38H3M0t0h+87w\n"
"GqCwFTdPfBJrIFjWf8ahpzGXThcVyD5DSkMOQoQbl1eo56saeXHrWa9nijFilhKm\n"
"ddSKdDdSZa1SDLHLltf/fGApXxdvbxyTed0tjid+YCIfylme++r/Q9gwo/l2pOAn\n"
"9g1WX9RGeQSVbvne/2XX\n"
"=I+8J\n"
"-----END PGP SIGNATURE-----\n";

const gchar *sig_message = "d0bb8a23da064efb222b1c8407711f231655460c6ed643b3ff311b6f92f99d69	repo/repo-index\n"
"ab33f19095252116ef503c4b7c3562410a1f4961a27c919f7e4efbb53e7df15a	control\n"
"c34d4eb3f7ef6343998e77a32f0fb85a78201229086190dddaf00e4a0ab17731	metainfo.xml\n"
"096ba20c95d07d96d5bb8de664cc6a2dd5bcf87ad4120978e3d3babedf0a2d3b	main-data.tar.xz\n";

void
test_keyring () {
	LiKeyring *kr;
	LiTrustLevel level;
	GError *error = NULL;
	gchar *tmp;
	gchar *fpr = NULL;

	kr = li_keyring_new ();

	/* validate signature */
	level = li_keyring_process_pkg_signature (kr, sig_signature, &tmp, &fpr, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (sig_message, ==, tmp);
	g_assert_cmpstr (fpr, ==, "D33A3F0CA16B0ACC51A60738494C8A5FBF4DECEB");
	g_free (tmp);
	g_assert (level == LI_TRUST_LEVEL_LOW);

	/* import that kley to the high-trust database */
	li_keyring_import_key (kr, fpr, LI_KEYRING_KIND_USER, &error);
	g_assert_no_error (error);
	g_free (fpr);

	/* check if we have a higher trust level now */
	level = li_keyring_process_pkg_signature (kr, sig_signature, NULL, NULL, &error);
	g_assert_no_error (error);

	//! We can't trust this yet, since GPGMe sometimes silently fails to import a key
	//! g_assert (level == LI_TRUST_LEVEL_HIGH);

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
	li_delete_dir_recursive ("/var/tmp/limba/tests");

	/* critical, error and warnings are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/Limba/Keyring", test_keyring);

	ret = g_test_run ();
	g_free (datadir);
	return ret;
}
