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

/**
 * SECTION:li-keyring
 * @short_description: Database of trusted keys as well as methods to verify signatures on Limba packages
 *
 * Limba maintains two keyrings: One with keys which are trusted explicitly (meaning that a human has explicitly
 * defined them as trusted), and one which is trusted implicitly (meaning that it contains keys of software which
 * has been installed on the system).
 */

#include "config.h"
#include "li-keyring.h"

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <gpgme.h>

#include "li-utils-private.h"

typedef struct _LiKeyringPrivate	LiKeyringPrivate;
struct _LiKeyringPrivate
{
	gchar *gpg_home_user;
	gchar *gpg_home_automatic;
	gchar *gpg_home_tmp;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiKeyring, li_keyring, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_keyring_get_instance_private (o))

#define LI_GPG_PROTOCOL GPGME_PROTOCOL_OpenPGP

/**
 * li_keyring_finalize:
 **/
static void
li_keyring_finalize (GObject *object)
{
	LiKeyring *kr = LI_KEYRING (object);
	LiKeyringPrivate *priv = GET_PRIVATE (kr);

	g_free (priv->gpg_home_user);
	g_free (priv->gpg_home_automatic);
	if (priv->gpg_home_tmp != NULL) {
		li_delete_dir_recursive (priv->gpg_home_tmp);
		g_free (priv->gpg_home_tmp);
	}

	G_OBJECT_CLASS (li_keyring_parent_class)->finalize (object);
}

/**
 * li_keyring_init:
 **/
static void
li_keyring_init (LiKeyring *kr)
{
	gpgme_error_t err;
	LiKeyringPrivate *priv = GET_PRIVATE (kr);

	gpgme_check_version (NULL);
	setlocale (LC_ALL, "");
	gpgme_set_locale (NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));
	err = gpgme_engine_check_version (LI_GPG_PROTOCOL);
	if (err != 0) {
		g_critical ("GPGMe engine version check failed: %s", gpgme_strerror (err));
		g_assert (err == 0);
	}

	priv->gpg_home_user = g_build_filename (LI_KEYRING_ROOT, "trusted", NULL);
	priv->gpg_home_automatic = g_build_filename (LI_KEYRING_ROOT, "automatic", NULL);
	priv->gpg_home_tmp = NULL;
}

/**
 * li_keyring_get_context:
 */
gpgme_ctx_t
li_keyring_get_context (LiKeyring *kr, LiKeyringKind kind)
{
	const gchar *home = NULL;
	gboolean tmpdir = FALSE;
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	LiKeyringPrivate *priv = GET_PRIVATE (kr);

	if (kind == LI_KEYRING_KIND_USER) {
		home = priv->gpg_home_user;
	} else if (kind == LI_KEYRING_KIND_AUTOMATIC) {
		home = priv->gpg_home_automatic;
	} else {
		/* we create these super-ugly temporary GPG home dirs to prevent a normal
		 * signature validation from tampering with our keyring (e.g. by occasionally
		 * importing a key to it)
		 * This has been done in several other projects, but we should do something
		 * less-hackish as soon as that option exists.
		 */
		if (priv->gpg_home_tmp != NULL) {
			li_delete_dir_recursive (priv->gpg_home_tmp);
			g_free (priv->gpg_home_tmp);
		}
		priv->gpg_home_tmp = g_build_filename ("/tmp", "gpg.tmp-XXXXXX", NULL);
		g_mkdtemp (priv->gpg_home_tmp);
		home = priv->gpg_home_tmp;
		tmpdir = TRUE;
	}

	if ((tmpdir) || (li_utils_is_root () && (!g_file_test (home, G_FILE_TEST_IS_DIR))))  {
		gchar *gpgconf_fname;
		_cleanup_free_ gchar *gpg_conf = NULL;
		/* Yes, this is as stupid as it looks... */
		if (kind == LI_KEYRING_KIND_NONE) {
			/* allow fetching keys when using a temporary keyring */
			gpg_conf = g_strdup ("# Options for GnuPG used by Limba \n\n"
				"no-greeting\n"
				"no-permission-warning\n"
				"no-default-keyring\n"
				"preserve-permissions\n"
				"lock-never\n"
				"no-expensive-trust-checks\n\n"
				"keyserver-options timeout=24\n"
				"keyserver-options auto-key-retrieve\n\n"
				"keyserver hkp://pool.sks-keyservers.net\n"
				"#keyserver hkp://keys.gnupg.net\n"
				"#keyserver hkp://keyring.debian.org\n");
		} else {
			/* We don't want any keyserver configured for system keyrings */
			gpg_conf = g_strdup ("# Options for GnuPG used by Limba \n\n"
				"no-greeting\n"
				"no-permission-warning\n"
				"no-default-keyring\n"
				"preserve-permissions\n"
				"lock-never\n"
				"trust-model direct\n"
				"no-expensive-trust-checks\n\n");
		}
		g_mkdir_with_parents (home, 0755);

		gpgconf_fname = g_build_filename (home, "gpg.conf", NULL);
		g_file_set_contents (gpgconf_fname, gpg_conf, -1, NULL);
		g_free (gpgconf_fname);

		g_debug ("Created new GPG home dir at %s", home);
	}

	err = gpgme_new (&ctx);
	g_assert (err == 0);

	err = gpgme_ctx_set_engine_info (ctx, LI_GPG_PROTOCOL, NULL, home);
	g_assert (err == 0);
	err = gpgme_set_protocol (ctx, LI_GPG_PROTOCOL);
	g_assert (err == 0);

	return ctx;
}

/**
 * li_keyring_lookup_key:
 */
static gpgme_key_t
li_keyring_lookup_key (gpgme_ctx_t ctx, const gchar *fpr, gboolean remote, GError **error)
{
	_cleanup_free_ gchar *full_fpr = NULL;
	gpgme_error_t err;
	gpgme_keylist_mode_t mode;
	gpgme_key_t key;

	if (g_str_has_prefix (fpr, "0x"))
		full_fpr = g_strdup (fpr);
	else
		full_fpr = g_strdup_printf ("0x%s", fpr);

	mode = gpgme_get_keylist_mode (ctx);
	/* using LOCAL and EXTERN together doesn't work for GPG 1.X. Ugh. */
	if (remote) {
		mode &= ~GPGME_KEYLIST_MODE_LOCAL;
		mode |= GPGME_KEYLIST_MODE_EXTERN;
		g_debug ("Remote lookup for GPG key: %s", full_fpr);
	} else {
		mode &= ~GPGME_KEYLIST_MODE_EXTERN;
		mode |= GPGME_KEYLIST_MODE_LOCAL;
		g_debug ("Local lookup for GPG key: %s", full_fpr);
	}

	err = gpgme_set_keylist_mode (ctx, mode);
	if (err != 0) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_LOOKUP,
			_("Key lookup failed: %s"),
			gpgme_strerror (err));
		return NULL;
	}

	err = gpgme_get_key (ctx, full_fpr, &key, FALSE);
	if(gpg_err_code (err) == GPG_ERR_EOF) {
		/* we couldn't find the key */
		return NULL;
	}

	if (err != 0) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_LOOKUP,
			_("Key lookup failed: %s"),
			gpgme_strerror (err));
		return NULL;
	}

	g_debug ("Found key for: %s", key->uids->name);

	return key;
}

/**
 * li_keyring_import_key:
 * @fpr: The fingerprint of the key to fetch.
 * @kind: The keyring type to add this key to
 *
 * Get a key matching the fingerprint and add it to the respective keyring.
 */
gboolean
li_keyring_import_key (LiKeyring *kr, const gchar *fpr, LiKeyringKind kind, GError **error)
{
	gpgme_ctx_t ctx_target;
	gpgme_ctx_t ctx_tmp;
	gpgme_key_t key;
	gpgme_error_t err;
	gpgme_key_t keys[2];
	gpgme_data_t key_data = NULL;
	gpgme_import_result_t ires;
	gchar *cmd;
	gpgme_engine_info_t engine;
	GError *tmp_error = NULL;

	ctx_target = li_keyring_get_context (kr, kind);
	ctx_tmp = li_keyring_get_context (kr, LI_KEYRING_KIND_NONE);

	/* check if we already have that key */
	key = li_keyring_lookup_key (ctx_target, fpr, FALSE, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		gpgme_release (ctx_tmp);
		gpgme_release (ctx_target);
		return FALSE;
	}
	if (key != NULL) {
		/* we already trust the key! */
		g_debug ("Key '%s' is already in the keyring.", fpr);
		gpgme_key_unref (key);
		gpgme_release (ctx_tmp);
		gpgme_release (ctx_target);
		return TRUE;
	}

	/* fetch key from remote */
	key = li_keyring_lookup_key (ctx_tmp, fpr, TRUE, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		gpgme_release (ctx_tmp);
		gpgme_release (ctx_target);
		return FALSE;
	}
	if (key == NULL) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_KEY_UNKNOWN,
			_("Key lookup failed, could not find remote key."));
		gpgme_release (ctx_tmp);
		gpgme_release (ctx_target);
		return FALSE;
	}

	keys[0] = key;
	keys[1] = NULL;

	/* add key to temporary keyring */
	err = gpgme_op_import_keys (ctx_tmp, keys);
	if (err != 0) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_IMPORT,
			_("Key import failed: %s"),
			gpgme_strerror (err));
		gpgme_key_unref (key);
		gpgme_release (ctx_tmp);
		gpgme_release (ctx_target);
		return FALSE;
	}

	/* FIXME: The key import above currently always fails, for unknown reason... We add a workaround here,
	 * which should be removed as soon as GPGMe is working for us (bug report has been filed). */
	engine = gpgme_ctx_get_engine_info (ctx_tmp);
	cmd = g_strdup_printf ("gpg2 --batch --no-tty --lc-ctype=C --homedir=%s --recv-key %s", engine->home_dir, fpr);
	system (cmd);
	g_free (cmd);

	err = gpgme_data_new (&key_data);
	if (err != 0) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_IMPORT,
			_("Key import failed: %s"),
			gpgme_strerror (err));
		gpgme_key_unref (key);
		gpgme_release (ctx_tmp);
		gpgme_release (ctx_target);
		return FALSE;
	}

	err = gpgme_op_export_keys (ctx_tmp, keys, 0, key_data);
	gpgme_release (ctx_tmp);
	if (err != 0) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_IMPORT,
			_("Key export failed: %s"),
			gpgme_strerror (err));
		gpgme_key_unref (key);
		gpgme_release (ctx_target);
		gpgme_data_release (key_data);
		return FALSE;
	}
	gpgme_key_unref (key);

	/* rewind manually, GPGMe doesn't do that for us */
	gpgme_data_seek (key_data, 0, SEEK_SET);

	err = gpgme_op_import (ctx_target, key_data);
	gpgme_data_release (key_data);
	if (err != 0) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_IMPORT,
			_("Importing of key failed: %s"),
			gpgme_strerror (err));
		gpgme_release (ctx_target);
		return FALSE;
	}

	ires = gpgme_op_import_result (ctx_target);
	if (!ires) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_IMPORT,
			_("Importing of key failed: %s"),
			_("No import result returned."));
		gpgme_release (ctx_target);
		return FALSE;
	}

	/* we tried to import one key, so one key should have been accepted */
	if (ires->considered != 1 || !ires->imports) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_IMPORT,
			_("Importing of key failed: %s"),
			_("Zero results returned."));
		gpgme_release (ctx_target);
		return FALSE;
	}

	gpgme_release (ctx_target);

	return TRUE;
}

/**
 * li_keyring_verify_clear_signature:
 *
 * Verifies a GPG signature.
 *
 * Returns: The data which was signed.
 */
gchar*
li_keyring_verify_clear_signature (LiKeyring *kr, LiKeyringKind kind, const gchar *sigtext, gchar **out_fpr, GError **error)
{
	gpgme_ctx_t ctx;
	gpgme_error_t err;
	gpgme_data_t sigdata = NULL;
	gpgme_data_t data = NULL;
	#define BUF_SIZE 512
	char buf[BUF_SIZE + 1];
	int ret;
	GString *str;
	gpgme_verify_result_t result;
	gpgme_signature_t sig;

	ctx = li_keyring_get_context (kr, kind);

	err = gpgme_data_new_from_mem (&sigdata, sigtext, strlen (sigtext), 1);
	if (err != 0) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_VERIFY,
			_("Signature validation failed: %s"),
			gpgme_strerror (err));
		gpgme_release (ctx);
		return NULL;
	}

	gpgme_data_new (&data);

	err = gpgme_op_verify (ctx, sigdata, NULL, data);
	if (err != 0) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_VERIFY,
			_("Signature validation failed: %s"),
			gpgme_strerror (err));
		gpgme_data_release (sigdata);
		gpgme_release (ctx);
		return NULL;
	}

	result = gpgme_op_verify_result (ctx);
	if (result == NULL) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_VERIFY,
			_("Signature validation failed: %s"),
			_("No result received."));
		gpgme_data_release (sigdata);
		gpgme_release (ctx);
		return NULL;
	}

	sig = result->signatures;
	if (sig == NULL) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_VERIFY,
			_("Signature validation failed. Signature is invalid or not a signature."));
		gpgme_data_release (sigdata);
		gpgme_data_release (data);
		gpgme_release (ctx);
		return NULL;
	}

	if (sig->status != GPG_ERR_NO_ERROR) {
		if (sig->status == GPG_ERR_NO_PUBKEY) {
			g_set_error (error,
				LI_KEYRING_ERROR,
				LI_KEYRING_ERROR_VERIFY,
				_("Could not verify signature: They key could not be found or downloaded."));
		} else {
			g_set_error (error,
				LI_KEYRING_ERROR,
				LI_KEYRING_ERROR_VERIFY,
				_("Signature validation failed. Signature is invalid. (%s)"),
				gpgme_strerror (sig->status));
		}
		gpgme_data_release (sigdata);
		gpgme_data_release (data);
		gpgme_release (ctx);
		return NULL;
	}

	str = g_string_new ("");
	ret = gpgme_data_seek (data, 0, SEEK_SET);
	while ((ret = gpgme_data_read (data, buf, BUF_SIZE)) > 0)
		g_string_append_len (str, buf, ret);

	if (out_fpr != NULL) {
		*out_fpr = g_strdup (sig->fpr);
	}

	gpgme_data_release (data);
	gpgme_data_release (sigdata);

	gpgme_release (ctx);

	return g_string_free (str, FALSE);
}

/**
 * li_keyring_process_signature:
 *
 * Validate the signature of an IPK package and check the trusted keyrings
 * to determine a trust-level for this package.
 */
LiTrustLevel
li_keyring_process_signature (LiKeyring *kr, const gchar *sigtext, gchar **out_data, gchar **out_fpr, GError **error)
{
	gchar *sdata;
	gchar *fpr = NULL;
	LiTrustLevel level;
	GError *error_ucheck = NULL;
	GError *tmp_error = NULL;

	/* Trust levels:
	 * None: Signature is broken, package is not trusted.
	 * Low: Signature is valid, but we don't trust they key it was signed with.
	 * Medium: Signature is implicitly trusted.
	 * High: Signature is explicitly trusted.
	 */

	/* can we validate the signature with keys in our trusted database? */
	sdata = li_keyring_verify_clear_signature (kr,
					LI_KEYRING_KIND_USER,
					sigtext,
					&fpr,
					&error_ucheck);
	level = LI_TRUST_LEVEL_HIGH;

	if (error_ucheck != NULL) {
		/* do we implicitly trust that key? */
		sdata = li_keyring_verify_clear_signature (kr,
					LI_KEYRING_KIND_USER,
					sigtext,
					&fpr,
					&tmp_error);

		level = LI_TRUST_LEVEL_MEDIUM;
		if (tmp_error == NULL) {
			g_error_free (error_ucheck);
		} else {
			g_propagate_error (error, tmp_error);
			return LI_TRUST_LEVEL_NONE;
		}
	}

	if (out_data != NULL)
		*out_data = sdata;
	else
		g_free (sdata);

	if (out_fpr != NULL)
		*out_fpr = g_strdup (fpr);


	if (fpr != NULL)
		g_free (fpr);

	return level;
}

/**
 * li_keyring_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_keyring_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiKeyringError");
	return quark;
}

/**
 * li_keyring_class_init:
 **/
static void
li_keyring_class_init (LiKeyringClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_keyring_finalize;
}

/**
 * li_keyring_new:
 *
 * Creates a new #LiKeyring.
 *
 * Returns: (transfer full): a #LiKeyring
 *
 **/
LiKeyring *
li_keyring_new (void)
{
	LiKeyring *kr;
	kr = g_object_new (LI_TYPE_KEYRING, NULL);
	return LI_KEYRING (kr);
}
