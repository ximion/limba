/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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
#include <stdlib.h>
#include "limba.h"

#include "li-keyring.h"

static gchar *datadir = NULL;

const gchar *sig_signature = "-----BEGIN PGP MESSAGE-----\n"
"Version: GnuPG v1\n\n"
"owEBbwOQ/JANAwACAdcrhfn5D9YPAa0BPmIDbXNnVpqARGNlNjg0NjQ2MTBhYWYx\n"
"YTFkMmNhNDQxZDY5Y2YwYzUzYzE0YzIxZjU5OGE4MjEzM2U2OTUyMmVkMDhiMmU5\n"
"ZmEJcmVwby9pbmRleAoxM2NkZmM3ZjA2ZjNhYjdlZjdiY2ZlOTJiYmRiODFjNWQ2\n"
"MzhhZDg1YjJjNzA0M2UzZGJmNzczNzViM2ZlM2RkCWNvbnRyb2wKYzM0ZDRlYjNm\n"
"N2VmNjM0Mzk5OGU3N2EzMmYwZmI4NWE3ODIwMTIyOTA4NjE5MGRkZGFmMDBlNGEw\n"
"YWIxNzczMQltZXRhaW5mby54bWwKM2Y5ZTkxMDI5NGU2NmI5OGM1ZTJiYjdiMDJm\n"
"ZjI3NTUwZTE4Nzg3M2I2YzhhZGJjOTk4YmJmOWY1ZjUxN2Q4ZAltYWluLWRhdGEu\n"
"dGFyLnh6CokCHAQAAQIABgUCVpqARAAKCRDXK4X5+Q/WD1P5D/4nQGiQ+gDBTKag\n"
"zJMEQcfVeOvxWlK8BP3IHOVPkHEMGhSA8XJgWaqKf/2UvJcdUtXIZ5U8VANOeKi8\n"
"hqrdT/wm672bL0GWj5tG6ht6VXSHGjgGpfxHRNPeGdIfvJX5ve7uKpgurRk/Lw+Y\n"
"tX9GXVqGRFoTvPFF2g703NaRx4xesGGU5Qs4Z/XHtHpZc4g4eHr/X/mDVmLlDUsu\n"
"wKl0pP/B8p9EvrlfQ5bveyHg9FnRnxvr4gVIik2lShvKr8EyIXxWJ5PFwNizA045\n"
"gMmlHaorTNHNSIbg5nN5Qwv7/HpqWEJSLVjMcY4iGfjuHNdmQepdSJUZFa6s7w5u\n"
"YpTSD+8auxH90g3eWjD3KGDdBKyf/xV9GavNibLPq7A3hZzuy0PMTDl8hKcYnvHi\n"
"oY4RslXuLe6V6e/CAi8ynFPuS3jVdpBlBlYhtRlHSPxJtFbtECvXecH6rEGIlztd\n"
"C4cCM1iqAxNsGCk0Pz5dcmZOBuclYhYhtZ9v6QodpT57rid7/1FxdJhjK8+URTFJ\n"
"TRvrkho9Iktl2jKlcUlOmD1KPwejmfcaeoG2CHqia5dtfbzuefFpqGNik45QFnVa\n"
"Il6Q/9khsQXdckdvc4FA9XHvoxiA2JzACGw90lox3cn3DT0UEiB2vpZ71wZyjaWc\n"
"NyN/NMqC2bW52VW8A720R4Xeza8b8g==\n"
"=7mBL\n"
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
	level = li_keyring_process_signature (kr, sig_signature, &tmp, &fpr, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (sig_message, ==, tmp);
	g_assert_cmpstr (fpr, ==, "D1E764E137B61E688EF3B249D72B85F9F90FD60F");
	g_free (tmp);
	g_assert (level == LI_TRUST_LEVEL_MEDIUM);

	/* import that key to the high-trust database */
	li_keyring_import_key (kr, fpr, LI_KEYRING_KIND_USER, &error);
	g_assert_no_error (error);
	g_free (fpr);

	/* check if we have a higher trust level now */
	level = li_keyring_process_signature (kr, sig_signature, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (level == LI_TRUST_LEVEL_HIGH);

	g_object_unref (kr);
}

int
main (int argc, char **argv)
{
	gchar *tmp;
	gchar *cmd;
	int ret;

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
	g_mkdir_with_parents ("/var/lib/limba/keyrings", 0755);
	cmd = g_strdup_printf ("cp -r '%s' /var/lib/limba/keyrings/automatic", tmp);
	g_assert (system (cmd) == 0); /* meh for call to system() - but okay for the testsuite */
	g_free (tmp);
	g_free (cmd);

	li_set_verbose_mode (TRUE);
	g_test_init (&argc, &argv, NULL);

	/* critical, error and warnings are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

	g_test_add_func ("/Limba/Keyring", test_keyring);

	ret = g_test_run ();
	g_free (datadir);

	return ret;
}
