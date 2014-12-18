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
#include <gpgme.h>

#include "li-utils-private.h"

typedef struct _LiKeyringPrivate	LiKeyringPrivate;
struct _LiKeyringPrivate
{
	gchar *gpg_home_user;
	gchar *gpg_home_automatic;
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

	G_OBJECT_CLASS (li_keyring_parent_class)->finalize (object);
}

/**
 * li_keyring_init:
 **/
static void
li_keyring_init (LiKeyring *kr)
{
	gpgme_error_t err;
	gchar *keyring_root;
	LiKeyringPrivate *priv = GET_PRIVATE (kr);

	gpgme_check_version (NULL);
	setlocale (LC_ALL, "");
	gpgme_set_locale (NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));
	err = gpgme_engine_check_version (LI_GPG_PROTOCOL);
	g_assert (err == 0);

	/* use a temporary keyring for unit-tests */
	if (li_get_unittestmode ()) {
		keyring_root = g_strdup ("/var/tmp/limba/tests/localstate/keyring");
	} else {
		keyring_root = g_strdup (LI_KEYRING_ROOT);
	}

	priv->gpg_home_user = g_build_filename (keyring_root, "trusted", NULL);
	priv->gpg_home_automatic = g_build_filename (keyring_root, "automatic", NULL);
	g_free (keyring_root);
}

/**
 * li_keyring_get_context:
 */
gpgme_ctx_t
li_keyring_get_context (LiKeyring *kr, LiKeyringKind kind)
{
	const gchar *home = NULL;
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	LiKeyringPrivate *priv = GET_PRIVATE (kr);

	if (kind == LI_KEYRING_KIND_USER)
		home = priv->gpg_home_user;
	else if (kind == LI_KEYRING_KIND_AUTOMATIC)
		home = priv->gpg_home_automatic;

	err = gpgme_new (&ctx);
	g_assert (err == 0);
	gpgme_set_protocol (ctx, LI_GPG_PROTOCOL);

	if (home == NULL) {
		return ctx;
	}

	if ((li_utils_is_root () || li_get_unittestmode ()) &&
		(!g_file_test (home, G_FILE_TEST_IS_DIR)))  {
		gchar *gpgconf_fname;
		const gchar *gpg_conf = "# Options for GnuPG used by Limba \n\n"
			"no-greeting\n"
			"no-permission-warning\n"
			"lock-never\n"
			"keyserver-options timeout=10\n\n"
			"keyserver hkp://keys.gnupg.net\n"
			"#keyserver hkp://keyring.debian.org\n\n"
			"keyserver-options auto-key-retrieve\n";
		g_mkdir_with_parents (home, 0755);

		gpgconf_fname = g_build_filename (home, "gpg.conf", NULL);
		g_file_set_contents (gpgconf_fname, gpg_conf, -1, NULL);
		g_free (gpgconf_fname);
	}
	gpgme_ctx_set_engine_info (ctx, LI_GPG_PROTOCOL, NULL, home);

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
				gpgme_strsource (err));
		return NULL;
	}

	err = gpgme_get_key (ctx, full_fpr, &key, 0);
	if(gpg_err_code (err) == GPG_ERR_EOF) {
		/* we couldn't find the key */
		return NULL;
	}
	if (err != 0) {
		g_set_error (error,
				LI_KEYRING_ERROR,
				LI_KEYRING_ERROR_LOOKUP,
				_("Key lookup failed: %s"),
				gpgme_strsource (err));
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
	gpgme_ctx_t ctx;
	gpgme_key_t key;
	gpgme_error_t err;
	gpgme_key_t *keys;
	gpgme_import_result_t ires;
	GError *tmp_error = NULL;

	ctx = li_keyring_get_context (kr, kind);

	/* check if we already have that key */
	key = li_keyring_lookup_key (ctx, fpr, FALSE, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	if (key != NULL) {
		/* we already have that key! */
		g_debug ("Key '%s' is already in the keyring.", fpr);
		gpgme_key_unref (key);
		gpgme_release (ctx);
		return TRUE;
	}

	key = li_keyring_lookup_key (ctx, fpr, TRUE, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	if (key == NULL) {
		g_set_error (error,
			LI_KEYRING_ERROR,
			LI_KEYRING_ERROR_KEY_UNKNOWN,
			_("Key lookup failed, could not find remote key."));
		gpgme_release (ctx);
		return FALSE;
	}

	keys = g_new0 (gpgme_key_t, 2 + 1);
	keys[0] = key;
	keys[1] = NULL;

	err = gpgme_op_import_keys (ctx, keys);
	if (err != 0) {
		g_set_error (error,
				LI_KEYRING_ERROR,
				LI_KEYRING_ERROR_IMPORT,
				_("Importing of key failed: %s"),
				gpgme_strsource (err));
		gpgme_key_unref (key);
		gpgme_release (ctx);
		return FALSE;
	}

	ires = gpgme_op_import_result (ctx);
	if (!ires) {
		g_set_error (error,
				LI_KEYRING_ERROR,
				LI_KEYRING_ERROR_IMPORT,
				_("Importing of key failed: %s"),
				_("No import result returned."));
		gpgme_key_unref (key);
		gpgme_release (ctx);
		return FALSE;
	}

	gpgme_key_unref (key);
	gpgme_release (ctx);

	return TRUE;
}

/**
 * li_keyring_verify_clear_signature:
 */
gchar*
li_keyring_verify_clear_signature (LiKeyring *kr, const gchar *sigtext, gchar **out_fpr, GError **error)
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

	ctx = li_keyring_get_context (kr, LI_KEYRING_KIND_NONE);

	err = gpgme_data_new_from_mem (&sigdata, sigtext, strlen (sigtext), 1);
	if (err != 0) {
		g_set_error (error,
				LI_KEYRING_ERROR,
				LI_KEYRING_ERROR_VERIFY,
				_("Signature validation failed: %s"),
				gpgme_strsource (err));
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
				gpgme_strsource (err));
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

	if (sig->status != GPG_ERR_NO_ERROR) {
		g_set_error (error,
				LI_KEYRING_ERROR,
				LI_KEYRING_ERROR_VERIFY,
				_("Signature validation failed. Signature is invalid. (%s)"),
				gpgme_strsource (sig->status));
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
 * li_keyring_process_pkg_signature:
 *
 * Validate the signature of an IPK package and check the trusted keyrings
 * to determine a trust-level for this package.
 */
LiTrustLevel
li_keyring_process_pkg_signature (LiKeyring *kr, const gchar *sigtext, gchar **out_data, gchar **out_fpr, GError **error)
{
	gchar *sdata;
	gchar *fpr = NULL;
	LiTrustLevel level;
	gpgme_ctx_t ctx = NULL;
	gpgme_key_t key = NULL;
	GError *tmp_error = NULL;

	sdata = li_keyring_verify_clear_signature (kr, sigtext, &fpr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return LI_TRUST_LEVEL_NONE;
	}

	if (out_data != NULL)
		*out_data = sdata;
	else
		g_free (sdata);

	if (out_fpr != NULL)
		*out_fpr = g_strdup (fpr);

	/* if we are here, we have at least low trust, since the signature is valid */
	level = LI_TRUST_LEVEL_LOW;

	/* do we have that key in our trusted database? */
	ctx = li_keyring_get_context (kr, LI_KEYRING_KIND_USER);
	key = li_keyring_lookup_key (ctx, fpr, FALSE, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}
	if (key != NULL) {
		/* the key is highly trusted */
		level = LI_TRUST_LEVEL_HIGH;
		goto out;
	}

	gpgme_release (ctx);

	/* do we implicitly trust that key? */
	ctx = li_keyring_get_context (kr, LI_KEYRING_KIND_AUTOMATIC);
	key = li_keyring_lookup_key (ctx, fpr, FALSE, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}
	if (key != NULL) {
		/* the key has a medium trust level */
		level = LI_TRUST_LEVEL_MEDIUM;
		goto out;
	}

out:
	if (key != NULL)
		gpgme_key_unref (key);
	if (ctx != NULL)
		gpgme_release (ctx);
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
