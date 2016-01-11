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

/**
 * SECTION:li-pkg-builder
 * @short_description: Creates Limba packages
 */

#include "config.h"
#include "li-pkg-builder.h"

#include <glib/gi18n-lib.h>
#include <archive_entry.h>
#include <archive.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <appstream.h>
#include <gpgme.h>
#include <locale.h>

#include "li-utils.h"
#include "li-utils-private.h"
#include "li-package.h"
#include "li-pkg-index.h"
#include "li-config-data.h"

typedef struct _LiPkgBuilderPrivate	LiPkgBuilderPrivate;
struct _LiPkgBuilderPrivate
{
	gchar *dir;
	gchar *gpg_key;
	gboolean sign_package;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPkgBuilder, li_pkg_builder, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (li_pkg_builder_get_instance_private (o))

/**
 * li_pkg_builder_finalize:
 **/
static void
li_pkg_builder_finalize (GObject *object)
{
	LiPkgBuilder *builder = LI_PKG_BUILDER (object);
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);

	g_free (priv->dir);

	G_OBJECT_CLASS (li_pkg_builder_parent_class)->finalize (object);
}

/**
 * li_pkg_builder_init:
 **/
static void
li_pkg_builder_init (LiPkgBuilder *builder)
{
	gpgme_error_t err;
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);

	priv->gpg_key = NULL;
	priv->sign_package = TRUE;

	/* initialize GPGMe */
	gpgme_check_version (NULL);
	setlocale (LC_ALL, "");
	gpgme_set_locale (NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));

	err = gpgme_engine_check_version (LI_GPG_PROTOCOL);
	g_assert (err == 0);
}

/**
 * li_get_package_fname:
 */
static gchar*
li_get_package_fname (const gchar *root_dir, const gchar *disk_fname)
{
	gchar *tmp;
	gchar *fname = NULL;
	g_autofree gchar *root_prefix = NULL;
	gint offset;

	if (g_str_has_prefix (disk_fname, root_dir)) {
		fname = g_strdup (disk_fname + strlen (root_dir) + 1);
	}

	root_prefix = g_strdup (LI_SW_ROOT_PREFIX + 1);

	if (fname != NULL) {
		if (g_str_has_prefix (fname, root_prefix)) {
			tmp = g_strdup (fname);
			g_free (fname);

			offset = strlen (root_prefix) + 1;
			if (strlen (tmp) > offset)
				fname = g_strdup (tmp + offset);
			else
				fname = NULL;
			g_free (tmp);
		}
	}

	if (fname == NULL)
		return g_strdup (disk_fname);
	else
		return fname;
}

/**
 * li_pkg_builder_write_payload:
 */
static void
li_pkg_builder_write_payload (const gchar *input_dir, const gchar *out_fname, LiPackageKind kind, gboolean auto_filter)
{
	GPtrArray *files;
	struct archive *a;
	struct archive_entry *entry;
	struct stat st;
	char buff[8192];
	int len;
	int fd;
	guint i;

	files = li_utils_find_files (input_dir, TRUE);

	a = archive_write_new ();
	archive_write_add_filter_xz (a);
	archive_write_set_format_pax_restricted (a);
	archive_write_open_filename (a, out_fname);


	for (i = 0; i < files->len; i++) {
		g_autofree gchar *ar_fname;
		const gchar *fname = (const gchar *) g_ptr_array_index (files, i);

		ar_fname = li_get_package_fname (input_dir, fname);
		if (auto_filter) {
			if (kind == LI_PACKAGE_KIND_DEVEL) {
				if (!g_str_has_prefix (ar_fname, "include/"))
					continue;
			} else {
				if (g_str_has_prefix (ar_fname, "include/"))
					continue;
			}
		}

		if (lstat (fname, &st) != 0) {
			g_warning ("Could not stat file '%s'. Skipping it.", fname);
			continue;
		}
		entry = archive_entry_new ();
		archive_entry_set_pathname (entry, ar_fname);
		archive_entry_copy_stat (entry, &st);

		/* handle symbolic links */
		if (S_ISLNK (st.st_mode)) {
			g_autofree gchar *linktarget = NULL;
			ssize_t r;

			linktarget = malloc (st.st_size + 1);
			r = readlink (fname, linktarget, st.st_size + 1);
			if (r < 0) {
				g_warning ("Could not follow symlink '%s', readlink failed. Skipping it.", fname);
				archive_entry_free (entry);
				continue;
			}
			if (r > st.st_size) {
				g_warning ("Could not follow symlink '%s', buffer too small. Skipping it.", fname);
				archive_entry_free (entry);
				continue;
			}
			linktarget[st.st_size] = '\0';

			archive_entry_set_symlink (entry, linktarget);
		}

		/* write file header in tarball */
		archive_write_header (a, entry);

		/* add data, in case we have a regular file */
		if (S_ISREG (st.st_mode)) {
			fd = open (fname, O_RDONLY);
			if (fd < 0) {
				g_warning ("Could not open file '%s' for reading. Skipping it.", fname);
				archive_entry_free (entry);
				continue;
			}

			len = read (fd, buff, sizeof (buff));
			while (len > 0) {
				archive_write_data (a, buff, len);
				len = read(fd, buff, sizeof (buff));
			}
			close (fd);
		}

		archive_entry_free (entry);
	}

	archive_write_close(a);
	archive_write_free(a);
}

/**
 * li_pkg_builder_add_embedded_packages:
 *
 * Returns: (transfer full): A path to the index file of embedded packages
 */
static gchar*
li_pkg_builder_add_embedded_packages (const gchar *tmp_dir, const gchar *repo_source, GPtrArray *files, GError **error)
{
	guint i;
	g_autofree gchar *pkgs_tmpdir = NULL;
	gchar *tmp;
	GPtrArray *packages;
	LiPkgIndex *idx;
	GError *tmp_error = NULL;

	pkgs_tmpdir = g_build_filename (tmp_dir, "repo", NULL);
	g_mkdir_with_parents (pkgs_tmpdir, 0775);

	packages = li_utils_find_files_matching (repo_source, "*.ipk", FALSE);
	if (packages == NULL)
		return NULL;

	idx = li_pkg_index_new ();
	for (i = 0; i < packages->len; i++) {
		LiPackage *pkg;
		LiPkgInfo *pki;
		gchar *hash;
		const gchar *fname = (const gchar *) g_ptr_array_index (packages, i);

		pkg = li_package_new ();
		li_package_open_file (pkg, fname, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_prefixed_error (error, tmp_error, "Unable to process external package '%s'.", fname);
			g_object_unref (pkg);
			return NULL;
		}

		pki = li_package_get_info (pkg);
		hash = li_compute_checksum_for_file (fname);
		li_pkg_info_set_checksum_sha256 (pki, hash);
		g_free (hash);

		li_pkg_index_add_package (idx, pki);

		/* create target filename */
		tmp = g_strdup_printf ("%s/%s-%s.ipk", pkgs_tmpdir, li_pkg_info_get_name (pki), li_pkg_info_get_version (pki));
		li_copy_file (fname, tmp, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_prefixed_error (error, tmp_error, "Unable to process external package '%s'.", li_pkg_info_get_name (pki));
			g_object_unref (pkg);
			g_free (tmp);
			return NULL;
		}

		g_ptr_array_add (files, tmp);
	}

	tmp = g_build_filename (tmp_dir, "repo", "index", NULL);
	li_pkg_index_save_to_file (idx, tmp);

	return tmp;
}

/**
 * li_pkg_builder_write_package:
 */
static void
li_pkg_builder_write_package (GPtrArray *files, const gchar *out_fname, GError **error)
{
	struct archive *a;
	struct archive_entry *entry;
	struct stat st;
	char buff[8192];
	int len;
	int fd;
	guint i;
	FILE *fp;

	fp = fopen (out_fname, "w");
	if (fp == NULL) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_WRITE,
				_("Could not open file '%s' for writing."), out_fname);
		return;
	}

	/* write magic number */
	if (fputs (LI_IPK_MAGIC, fp) < 0) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_WRITE,
				_("Could not write to file: '%s'"), out_fname);
		fclose (fp);
		return;
	}

	a = archive_write_new ();
	archive_write_add_filter_gzip (a);
	archive_write_set_format_pax_restricted (a);
	archive_write_open_FILE (a, fp);

	for (i = 0; i < files->len; i++) {
		gchar *ar_fname;
		const gchar *fname = (const gchar *) g_ptr_array_index (files, i);

		ar_fname = g_path_get_basename (fname);

		/* sort the repository files into their subdirectory */
		if (g_str_has_suffix (fname, "repo/index")) {
			g_free (ar_fname);
			ar_fname = g_strdup ("repo/index");
		} else if (g_str_has_suffix (ar_fname, ".ipk")) {
			gchar *tmp = ar_fname;
			ar_fname = g_strdup_printf ("repo/%s", tmp);
			g_free (tmp);
		}

		stat (fname, &st);
		entry = archive_entry_new ();
		archive_entry_set_pathname (entry, ar_fname);
		g_free (ar_fname);

		archive_entry_set_size (entry, st.st_size);
		archive_entry_set_filetype (entry, AE_IFREG);
		archive_entry_set_perm (entry, 0644);
		archive_write_header (a, entry);

		fd = open (fname, O_RDONLY);
		if (fd < 0) {
			g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_WRITE,
				_("Could not open metadata-file '%s' for reading."), fname);
			archive_entry_free (entry);
			return;
		}

		len = read (fd, buff, sizeof (buff));
		while (len > 0) {
			archive_write_data (a, buff, len);
			len = read(fd, buff, sizeof (buff));
		}

		close (fd);
		archive_entry_free (entry);
	}

	archive_write_close (a);
	archive_write_free (a);
	fclose (fp);
}

/**
 * li_print_gpgsign_result:
 */
static void
li_print_gpgsign_result (gpgme_ctx_t ctx, gpgme_sign_result_t result, gpgme_sig_mode_t type)
{
	gpgme_invalid_key_t invkey;
	gpgme_new_signature_t sig;
	g_autofree gchar *short_fpr = NULL;
	gpgme_key_t key;

	for (invkey = result->invalid_signers; invkey; invkey = invkey->next)
		g_debug ("Signing key `%s' not used: %s <%s>",
				invkey->fpr, gpg_strerror (invkey->reason), gpg_strsource (invkey->reason));

	for (sig = result->signatures; sig; sig = sig->next) {
		g_debug ("Key fingerprint: %s", sig->fpr);
		g_debug ("Signature type : %d", sig->type);
		g_debug ("Public key algo: %d", sig->pubkey_algo);
		g_debug ("Hash algo .....: %d", sig->hash_algo);
		g_debug ("Creation time .: %ld", sig->timestamp);
		g_debug ("Sig class .....: 0x%u", sig->sig_class);

		if (short_fpr != NULL)
			g_free (short_fpr);
		short_fpr = g_strdup (&sig->fpr[strlen(sig->fpr)-8]);

		if (gpgme_get_key (ctx, sig->fpr, &key, 0) == 0) {
			if (key->uids != NULL)
				g_debug ("Signed for \"%s\" [0x%s]", key->uids->uid, short_fpr);
			gpgme_key_unref (key);
		} else {
			g_debug ("Package signed for 0x%s", short_fpr);
		}
	}
}

/**
 * li_pkg_builder_sign_data:
 *
 * Returns: Signed text, %NULL on error
 */
gpgme_data_t
li_pkg_builder_sign_data (LiPkgBuilder *builder, const gchar *data, gpgme_sig_mode_t sigmode, GError **error)
{
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	gpgme_data_t din, dout;
	gpgme_sign_result_t sig_res;
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);

	err = gpgme_new (&ctx);
	if (err != 0) {
		g_set_error (error,
			LI_BUILDER_ERROR,
			LI_BUILDER_ERROR_SIGN,
			_("Signing of package failed (init): %s"),
			gpgme_strerror (err));
		gpgme_release (ctx);
		return NULL;
	}

	gpgme_set_protocol (ctx, LI_GPG_PROTOCOL);
	gpgme_set_armor (ctx, TRUE);

	if (priv->gpg_key != NULL) {
		gpgme_key_t akey;

		err = gpgme_get_key (ctx, priv->gpg_key, &akey, 1);
		if (err != 0) {
			g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_SIGN,
				_("Signing of package failed (get-key): %s"),
				gpgme_strerror (err));
			gpgme_release (ctx);
			return NULL;
		}

		err = gpgme_signers_add (ctx, akey);
		if (err != 0) {
			g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_SIGN,
				_("Signing of package failed (signers-add): %s"),
				gpgme_strerror (err));
			gpgme_release (ctx);
			return NULL;
		}

		gpgme_key_unref (akey);
	}

	err = gpgme_data_new_from_mem (&din, data, strlen (data), 0);
	if (err != 0) {
		g_set_error (error,
			LI_BUILDER_ERROR,
			LI_BUILDER_ERROR_SIGN,
			_("Signing of package failed: %s"),
			gpgme_strerror (err));
		gpgme_data_release (din);
		gpgme_release (ctx);
		return NULL;
	}

	err = gpgme_data_new (&dout);
	if (err != 0) {
		g_set_error (error,
			LI_BUILDER_ERROR,
			LI_BUILDER_ERROR_SIGN,
			_("Signing of package failed: %s"),
			gpgme_strerror (err));
		gpgme_data_release (din);
		gpgme_release (ctx);
		return NULL;
	}

	err = gpgme_op_sign (ctx, din, dout, sigmode);
	sig_res = gpgme_op_sign_result (ctx);
	if (sig_res)
		li_print_gpgsign_result (ctx, sig_res, sigmode);

	gpgme_data_release (din);
	gpgme_release (ctx);

	if (err != 0) {
		g_set_error (error,
			LI_BUILDER_ERROR,
			LI_BUILDER_ERROR_SIGN,
			_("Signing of package failed (sign): %s"),
			gpgme_strerror (err));
		return NULL;
	}

	return dout;
}

/**
 * li_pkg_builder_sign_package:
 */
static gchar*
li_pkg_builder_sign_package (LiPkgBuilder *builder, const gchar *tmp_dir, GPtrArray *sign_files, GError **error)
{
	guint i;
	GString *index_str;
	g_autofree gchar *indexdata = NULL;
	gpgme_data_t sigdata;

	#define BUF_SIZE 512
	gchar *sig_fname;
	char buf[BUF_SIZE + 1];
	FILE *file;
	gint ret;
	GError *tmp_error = NULL;

	index_str = g_string_new ("");
	for (i = 0; i < sign_files->len; i++) {
		gchar *checksum;
		gchar *internal_name;
		const gchar *fname = (const gchar *) g_ptr_array_index (sign_files, i);

		if (g_str_has_prefix (fname, tmp_dir)) {
			internal_name = g_strdup (&fname[strlen (tmp_dir) + 1]);
		} else {
			internal_name = g_path_get_basename (fname);
		}

		checksum = li_compute_checksum_for_file (fname);
		if (checksum == NULL) {
			g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_SIGN,
				_("Unable to calculate checksum for: %s"),
				internal_name);
			g_free (internal_name);
			return NULL;
		}

		g_string_append_printf (index_str, "%s\t%s\n", checksum, internal_name);
		g_free (checksum);
		g_free (internal_name);
	}
	indexdata = g_string_free (index_str, FALSE);

	sigdata = li_pkg_builder_sign_data (builder, indexdata, GPGME_SIG_MODE_NORMAL, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}
	g_print ("%s\n", _("Package signed."));

	sig_fname = g_build_filename (tmp_dir, "_signature", NULL);
	file = fopen (sig_fname, "w");
	if (file == NULL) {
		g_set_error (error,
			LI_BUILDER_ERROR,
			LI_BUILDER_ERROR_SIGN,
			_("Unable to write signature: %s"),
			g_strerror (errno));
		g_free (sig_fname);
		gpgme_data_release (sigdata);
		return NULL;
	}

	gpgme_data_seek (sigdata, 0, SEEK_SET);
	while ((ret = gpgme_data_read (sigdata, buf, BUF_SIZE)) > 0)
		fwrite (buf, ret, 1, file);
	fclose (file);

	gpgme_data_release (sigdata);

	return sig_fname;
}

/**
 * li_pkg_builder_filelist_entry_for_pkgfile:
 */
static gchar*
li_pkg_builder_filelist_entry_for_pkgfile (LiPkgBuilder *builder, const gchar *pkg_fname)
{
	gchar *tmp;
	gchar *tmp2;
	gchar *tmp3;

	if (!g_file_test (pkg_fname, G_FILE_TEST_EXISTS))
		return NULL;

	tmp2 = g_path_get_basename (pkg_fname);
	tmp3 = li_compute_checksum_for_file (pkg_fname);
	tmp = g_strdup_printf ("%s %s", tmp3, tmp2);
	g_free (tmp2);
	g_free (tmp3);

	return tmp;
}

/**
 * li_pkg_builder_write_dsc_file:
 *
 * The .dsc description is meant to be used to upload one or more packages
 * to a remote repository.
 * This function generates a basic .dsc file for the new package.
 */
void
li_pkg_builder_write_dsc_file (LiPkgBuilder *builder, const gchar *pkg_fname_rt, const gchar *pkg_fname_sdk, LiPkgInfo *ctl, GError **error)
{
	g_autoptr(LiConfigData) cdata = NULL;
	g_autofree gchar *fname = NULL;
	g_autofree gchar *email = NULL;
	g_autofree gchar *username = NULL;
	g_autofree gchar *target_repo = NULL;
	gchar *tmp;
	gchar *tmp2;
	gchar *tmp3;

	gpgme_data_t sigdata;
	#define BUF_SIZE 512
	char buf[BUF_SIZE + 1];
	FILE *file;
	gint ret;
	GError *tmp_error = NULL;

	cdata = li_config_data_new ();

	/* set basic information */
	li_config_data_set_value (cdata, "Limba-Version", VERSION);

	tmp2 = li_pkg_builder_filelist_entry_for_pkgfile (builder, pkg_fname_rt);
	tmp3 = li_pkg_builder_filelist_entry_for_pkgfile (builder, pkg_fname_sdk);
	if (tmp3 == NULL)
		tmp = g_strdup (tmp2);
	else
		tmp = g_strdup_printf ("%s\n%s", tmp2, tmp3);
	li_config_data_set_value (cdata, "Files", tmp);
	g_free (tmp);
	g_free (tmp2);
	g_free (tmp3);
	tmp = NULL;

	/* set uploader field */
	email = li_env_get_user_email ();
	username = li_env_get_user_fullname ();
	target_repo = li_env_get_target_repo ();

	if (username == NULL) {
		if (email != NULL)
			tmp = g_strdup (email);
	} else {
		if (email != NULL)
			tmp = g_strdup_printf ("%s <%s>", username, email);
	}
	if (tmp != NULL) {
		li_config_data_set_value (cdata, "Uploader", tmp);
		g_free (tmp);
	}

	/* set target repository */
	li_config_data_set_value (cdata, "Target", target_repo);

	tmp = li_config_data_get_data (cdata);
	sigdata = li_pkg_builder_sign_data (builder, tmp, GPGME_SIG_MODE_CLEAR, &tmp_error);
	g_free (tmp);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}
	g_print ("%s\n", _("DSC file signed."));

	fname = g_strdup_printf ("%s.dsc", pkg_fname_rt);
	tmp = li_config_data_get_data (cdata);

	file = fopen (fname, "w");
	if (file == NULL) {
		g_set_error (error,
			LI_BUILDER_ERROR,
			LI_BUILDER_ERROR_SIGN,
			_("Unable to write signature on dsc file: %s"),
			g_strerror (errno));
		gpgme_data_release (sigdata);
		return;
	}

	gpgme_data_seek (sigdata, 0, SEEK_SET);
	while ((ret = gpgme_data_read (sigdata, buf, BUF_SIZE)) > 0)
		fwrite (buf, ret, 1, file);
	fclose (file);

	gpgme_data_release (sigdata);
}

/**
 * li_pkg_builder_build_package_with_details:
 *
 * Helper function, to build a package with given parameters.
 * This function expects all its parameters to be valid!
 */
static gboolean
li_pkg_builder_build_package_with_details (LiPkgBuilder *builder, LiPkgInfo *ctl, LiPackageKind kind, AsComponent *cpt, const gchar *payload_root, const gchar *pkg_fname, gboolean split_sdk, GError **error)
{
	g_autofree gchar *tmp_dir = NULL;
	g_autofree gchar *ctl_fname = NULL;
	g_autofree gchar *asdata_fname = NULL;

	g_autofree gchar *payload_file = NULL;
	g_autofree gchar *sig_fname = NULL;
	g_autoptr(GPtrArray) files = NULL;
	g_autoptr(GPtrArray) sign_files = NULL;
	g_autoptr (AsMetadata) metad = NULL;

	g_autofree gchar *cpt_orig_id = NULL;
	g_autofree gchar *orig_deps = NULL;
	g_autofree gchar *orig_sdk_deps = NULL;

	GError *tmp_error = NULL;
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);

	tmp_dir = li_utils_get_tmp_dir ("build");
	payload_file = g_build_filename (tmp_dir, "main-data.tar.xz", NULL);

	if ((split_sdk) && (kind == LI_PACKAGE_KIND_DEVEL)) {
		g_autofree gchar *tmp = NULL;

		tmp = g_build_filename (payload_root, "include", NULL);
		if (!g_file_test (tmp, G_FILE_TEST_IS_DIR)) {
			g_free (tmp);
			tmp = g_build_filename (payload_root, "app", "include", NULL);
			if (!g_file_test (tmp, G_FILE_TEST_IS_DIR)) {
				/* no headers? No automatically built SDK package! */
				return TRUE;
			}
		}
	}

	/* create payload */
	li_pkg_builder_write_payload (payload_root, payload_file, kind, split_sdk);

	/* prepare component metadata */
	metad = as_metadata_new ();
	as_metadata_set_locale (metad, "ALL");
	asdata_fname = g_build_filename (tmp_dir, "metainfo.xml", NULL);
	cpt_orig_id = g_strdup (as_component_get_id (cpt));

	/* development components must have an ID in the form of <cptid>.sdk, e.g. io.qt.Qt5Core.sdk */
	if (kind == LI_PACKAGE_KIND_DEVEL) {
		g_autofree gchar *sdk_id = NULL;

		sdk_id = g_strdup_printf ("%s.sdk", cpt_orig_id);
		as_component_set_id (cpt, sdk_id);
	}

	as_metadata_add_component (metad, cpt);
	as_metadata_save_upstream_xml (metad, asdata_fname, &tmp_error);
	as_component_set_id (cpt, cpt_orig_id);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* construct package contents */
	files = g_ptr_array_new_with_free_func (g_free);
	sign_files = g_ptr_array_new ();

	/* check if we should embed a repository in the package structure */
	if (kind == LI_PACKAGE_KIND_COMMON) {
		g_autofree gchar *repo_root = NULL;

		repo_root = g_build_filename (payload_root, "..", "repo", NULL);
		if (g_file_test (repo_root, G_FILE_TEST_IS_DIR)) {
			gchar *repo_fname;

			/* we have a dependency repo, embed extra packages */
			repo_fname = li_pkg_builder_add_embedded_packages (tmp_dir, repo_root, files, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return FALSE;
			}
			g_ptr_array_add (files, repo_fname);
			/* we need to sign the repo file */
			g_ptr_array_add (sign_files, repo_fname);
		}
	}

	/* set the package type we're building here */
	li_pkg_info_set_kind (ctl, kind);

	/* handle arch:any notation (resolve to current arch) */
	if (g_strcmp0 (li_pkg_info_get_architecture (ctl), "any") == 0) {
		gchar *arch;
		arch = li_get_current_arch_h ();
		li_pkg_info_set_architecture (ctl, arch);
		g_free (arch);
	}

	/* Ensure SDK packages always depend on their runtime package */
	orig_deps = g_strdup (li_pkg_info_get_dependencies (ctl));
	orig_sdk_deps = g_strdup (li_pkg_info_get_sdk_dependencies (ctl));
	if (kind == LI_PACKAGE_KIND_DEVEL) {
		g_autofree gchar *tmp = NULL;

		if ((li_pkg_info_get_sdk_dependencies (ctl) == NULL) || (g_strcmp0 (li_pkg_info_get_sdk_dependencies (ctl), "") == 0)) {
			tmp = g_strdup_printf ("%s (== %s)",
						cpt_orig_id,
						li_get_last_version_from_component (cpt));
		} else {
			tmp = g_strdup_printf ("%s (== %s), %s",
						cpt_orig_id,
						li_get_last_version_from_component (cpt),
						li_pkg_info_get_sdk_dependencies (ctl));
		}
		li_pkg_info_set_dependencies (ctl, tmp);
	}

	/* save our new control metadata */
	li_pkg_info_set_sdk_dependencies (ctl, NULL);
	ctl_fname = g_build_filename (tmp_dir, "control", NULL);
	li_pkg_info_save_to_file (ctl, ctl_fname);

	/* restore LiPkgInfo to previous state */
	li_pkg_info_set_dependencies (ctl, orig_deps);
	li_pkg_info_set_sdk_dependencies (ctl, orig_sdk_deps);

	/* we want these files in the package */
	g_ptr_array_add (files, g_strdup (ctl_fname));
	g_ptr_array_add (files, g_strdup (asdata_fname));
	g_ptr_array_add (files, g_strdup (payload_file));

	/* these files need to be signed in order to verify the whole package */
	g_ptr_array_add (sign_files, ctl_fname);
	g_ptr_array_add (sign_files, asdata_fname);
	g_ptr_array_add (sign_files, payload_file);

	if (priv->sign_package) {
		sig_fname = li_pkg_builder_sign_package (builder, tmp_dir, sign_files, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}
		g_ptr_array_add (files, g_strdup (sig_fname));
	}

	/* write package */
	li_pkg_builder_write_package (files, pkg_fname, &tmp_error);
	g_ptr_array_unref (files);

	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* cleanup temporary dir */
	li_delete_dir_recursive (tmp_dir);

	return TRUE;
}

/**
 * li_pkg_builder_create_package_from_dir:
 */
gboolean
li_pkg_builder_create_package_from_dir (LiPkgBuilder *builder, const gchar *dir, const gchar *out_fname, GError **error)
{
	g_autofree gchar *payload_root_rt = NULL;
	g_autofree gchar *payload_root_sdk = NULL;
	g_autofree gchar *asdata_fname = NULL;

	g_autofree gchar *pkg_fname_rt = NULL;
	g_autofree gchar *pkg_fname_sdk = NULL;
	g_autoptr(GFile) ctlfile = NULL;
	g_autoptr(LiPkgInfo) ctl = NULL;
	g_autoptr(AsComponent) cpt = NULL;
	g_autoptr(AsMetadata) mdata = NULL;
	g_autoptr(GFile) asfile = NULL;
	gboolean auto_sdkpkg;
	GError *tmp_error = NULL;
	gchar *ctlfile_fname;

	ctlfile_fname = g_build_filename (dir, "control", NULL);
	ctlfile = g_file_new_for_path (ctlfile_fname);
	g_free (ctlfile_fname);
	if (!g_file_query_exists (ctlfile, NULL)) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_NOT_FOUND,
				_("Could not find control file for the archive!"));
		return FALSE;
	}
	ctl = li_pkg_info_new ();
	li_pkg_info_load_file (ctl, ctlfile, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* load AppStream metadata */
	asdata_fname = g_build_filename (dir, "metainfo.xml", NULL);
	asfile = g_file_new_for_path (asdata_fname);
	if (!g_file_query_exists (asfile, NULL)) {
		g_set_error (error,
			LI_BUILDER_ERROR,
			LI_BUILDER_ERROR_FAILED,
			_("Could not build package: AppStream metadata is missing."));
		return FALSE;
	}

	mdata = as_metadata_new ();
	as_metadata_set_locale (mdata, "ALL");

	as_metadata_parse_file (mdata, asfile, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	cpt = as_metadata_get_component (mdata);
	g_object_ref (cpt);

	if (out_fname == NULL) {
		gchar *tmp;
		const gchar *version;

		/* we need to auto-generate a package filename */
		tmp = li_str_replace (as_component_get_name (cpt), " ", "");
		version = li_get_last_version_from_component (cpt);
		if (version != NULL) {
			pkg_fname_rt  = g_strdup_printf ("%s/%s-%s.ipk", dir, tmp, version);
			pkg_fname_sdk = g_strdup_printf ("%s/%s-%s.devel.ipk", dir, tmp, version);
		} else {
			pkg_fname_rt  = g_strdup_printf ("%s/%s.ipk", dir, tmp);
			pkg_fname_sdk = g_strdup_printf ("%s/%s.devel.ipk", dir, tmp);
		}
		g_free (tmp);
	} else {
		g_autofree gchar *base_fname;
		g_autofree gchar *dirname;

		pkg_fname_rt = g_strdup (out_fname);

		base_fname = g_path_get_basename (out_fname);
		dirname = g_path_get_dirname (out_fname);
		pkg_fname_sdk = g_strdup_printf ("%s/devel-%s", dirname, base_fname);
	}

	/* search for runtime payload */
	auto_sdkpkg = TRUE;
	payload_root_rt = g_build_filename (dir, "target", NULL);
	if (!g_file_test (payload_root_rt, G_FILE_TEST_IS_DIR)) {
		/* we don't automatically build an SDK package anymore */
		auto_sdkpkg = FALSE;

		g_free (payload_root_rt);
		payload_root_rt = g_build_filename (dir, "rt.target", NULL);
		if (!g_file_test (payload_root_rt, G_FILE_TEST_IS_DIR)) {
			g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_NOT_FOUND,
				_("Could not find payload data in the 'target' or 'rt.target' subdirectory."));
			return FALSE;
		}
	}

	/* search if we have a dedicated SDK payload */
	payload_root_sdk = g_build_filename (dir, "sdk.target", NULL);
	if (!g_file_test (payload_root_sdk, G_FILE_TEST_IS_DIR)) {
		g_free (payload_root_sdk);
		payload_root_sdk = NULL;
	}

	if (auto_sdkpkg) {
		/* build package, automatically detect if we need a development package */
		li_pkg_builder_build_package_with_details (builder, ctl, LI_PACKAGE_KIND_COMMON, cpt, payload_root_rt, pkg_fname_rt, TRUE, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}

		/* run the development package build (will return no package if no SDK components are auto-detected) */
		li_pkg_builder_build_package_with_details (builder, ctl, LI_PACKAGE_KIND_DEVEL, cpt, payload_root_rt, pkg_fname_sdk, TRUE, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}

	} else {
		/* build RT package */
		li_pkg_builder_build_package_with_details (builder, ctl, LI_PACKAGE_KIND_COMMON, cpt, payload_root_rt, pkg_fname_rt, FALSE, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}

		if (payload_root_sdk != NULL) {
			/* build SDK package */
			li_pkg_builder_build_package_with_details (builder, ctl, LI_PACKAGE_KIND_DEVEL, cpt, payload_root_sdk, pkg_fname_sdk, FALSE, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return FALSE;
			}
		}
	}

	/* write dsc file */
	li_pkg_builder_write_dsc_file (builder, pkg_fname_rt, pkg_fname_sdk, ctl, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

/**
 * li_pkg_builder_get_sign_package:
 */
gboolean
li_pkg_builder_get_sign_package (LiPkgBuilder *builder)
{
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);
	return priv->sign_package;
}

/**
 * li_pkg_builder_set_sign_package:
 */
void
li_pkg_builder_set_sign_package (LiPkgBuilder *builder, gboolean sign)
{
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);
	priv->sign_package = sign;
}

/**
 * li_builder_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_builder_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiBuilderError");
	return quark;
}

/**
 * li_pkg_builder_class_init:
 **/
static void
li_pkg_builder_class_init (LiPkgBuilderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_pkg_builder_finalize;
}

/**
 * li_pkg_builder_new:
 *
 * Creates a new #LiPkgBuilder.
 *
 * Returns: (transfer full): a #LiPkgBuilder
 *
 **/
LiPkgBuilder *
li_pkg_builder_new (void)
{
	LiPkgBuilder *builder;
	builder = g_object_new (LI_TYPE_PKG_BUILDER, NULL);
	return LI_PKG_BUILDER (builder);
}
