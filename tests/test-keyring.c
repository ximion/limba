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

const gchar *sig_signature = "-----BEGIN PGP MESSAGE-----\n"
"Version: GnuPG v2\n\n"
"owEBbAOT/JANAwACAUlMil+/TezrAcvAe2IAVJNGxmNlNjg0NjQ2MTBhYWYxYTFk\n"
"MmNhNDQxZDY5Y2YwYzUzYzE0YzIxZjU5OGE4MjEzM2U2OTUyMmVkMDhiMmU5ZmEJ\n"
"cmVwby9pbmRleAoxM2NkZmM3ZjA2ZjNhYjdlZjdiY2ZlOTJiYmRiODFjNWQ2Mzhh\n"
"ZDg1YjJjNzA0M2UzZGJmNzczNzViM2ZlM2RkCWNvbnRyb2wKYzM0ZDRlYjNmN2Vm\n"
"NjM0Mzk5OGU3N2EzMmYwZmI4NWE3ODIwMTIyOTA4NjE5MGRkZGFmMDBlNGEwYWIx\n"
"NzczMQltZXRhaW5mby54bWwKM2Y5ZTkxMDI5NGU2NmI5OGM1ZTJiYjdiMDJmZjI3\n"
"NTUwZTE4Nzg3M2I2YzhhZGJjOTk4YmJmOWY1ZjUxN2Q4ZAltYWluLWRhdGEudGFy\n"
"Lnh6CokCHAQAAQIABgUCVJNGxgAKCRBJTIpfv03s6+xmD/0TgAdrxSuAaovBvghM\n"
"VKlPgVSH2c7M7wO6PcLTHhUgAfll75kIIbdNT4CDhjkL3jMK5T4orxGBRWxp9rTH\n"
"FkoorWPiJ2OHggRfAXdYmkIqSQlDGdjq2a3U5NCXxa6JqFR65qpNFKBhfgPq89N2\n"
"ZwV3BtO3b5hGs1bgLmTgyUQ4MdNvrVrv1mT9UijVk620Vom2Z0oKV33SKhvHi8Ju\n"
"jjWI9wdv4zI7DSsFsATLz5XyJn/BfB+h49sEEEpBeFye7I0vJDZqm3MTsJBEnUhc\n"
"tTNfaHVK0IuYKzZaO75tp/xJWr7OIUT9PnPOiwaRwx5tWxHBKa/fcD1tJjoSBCfp\n"
"/UT3MkAuo1gbZUafJrbdqlR2KxOwEVMWKEqkeQhW2RVDQ55TBbbbF6A0pQV/hoj9\n"
"7b7ybmgE0Cm5VTPSkMDG8NaygtBij0NjuN2DPyQNfKgwR4PSTChQfmcrlvKh673G\n"
"NwgHk9m5HS1wjW+mKa1IiZP9O0UIvJOp2o6zmvE4k3kkxXN7DWzaGzd3pRRgGYxn\n"
"69uyfl8FE62HaVJA4fLX1H7ZPnteh47e73H/5YTjKqKcN3cJGaaxmmg2rtyaq79J\n"
"OR5HF5Zkh3ogx1mrQZnETeskOJoAWpSIxr/YbQ+OxRwwUYKLMTk3GfIC/nDZbgdj\n"
"FMvyzfF5CkHUZRvHP2jkyt0tBw==\n"
"=Acpp\n"
"-----END PGP MESSAGE-----\n";

const gchar *sig_message = "ce68464610aaf1a1d2ca441d69cf0c53c14c21f598a82133e69522ed08b2e9fa\trepo/index\n"
"13cdfc7f06f3ab7ef7bcfe92bbdb81c5d638ad85b2c7043e3dbf77375b3fe3dd\tcontrol\n"
"c34d4eb3f7ef6343998e77a32f0fb85a78201229086190dddaf00e4a0ab17731\tmetainfo.xml\n"
"3f9e910294e66b98c5e2bb7b02ff27550e187873b6c8adbc998bbf9f5f517d8d\tmain-data.tar.xz\n";

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
