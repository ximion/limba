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

typedef struct _LiPkgBuilderPrivate	LiPkgBuilderPrivate;
struct _LiPkgBuilderPrivate
{
	gchar *dir;
	gchar *gpg_key;
	gboolean sign_package;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPkgBuilder, li_pkg_builder, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_pkg_builder_get_instance_private (o))

#define LI_GPG_PROTOCOL GPGME_PROTOCOL_OpenPGP

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
	gint offset;

	if (g_str_has_prefix (disk_fname, root_dir)) {
		fname = g_strdup (disk_fname + strlen (root_dir));
	}
	if (fname != NULL) {
		if (g_str_has_prefix (fname, LI_SW_ROOT_PREFIX)) {
			tmp = g_strdup (fname);
			g_free (fname);

			offset = strlen (LI_SW_ROOT_PREFIX) + 1;
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
li_pkg_builder_write_payload (const gchar *input_dir, const gchar *out_fname)
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
		gchar *ar_fname;
		const gchar *fname = (const gchar *) g_ptr_array_index (files, i);

		ar_fname = li_get_package_fname (input_dir, fname);

		stat(fname, &st);
		entry = archive_entry_new ();
		archive_entry_set_pathname (entry, ar_fname);
		g_free (ar_fname);

		archive_entry_copy_stat (entry, &st);
		archive_write_header (a, entry);

		fd = open (fname, O_RDONLY);
		len = read (fd, buff, sizeof (buff));
		while (len > 0) {
			archive_write_data (a, buff, len);
			len = read(fd, buff, sizeof (buff));
		}
		close (fd);
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
	_cleanup_free_ gchar *pkgs_tmpdir = NULL;
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
	_cleanup_free_ gchar *short_fpr = NULL;
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
				g_print(_("Package signed for \"%s\" [0x%s]\n"), key->uids->uid, short_fpr);
			gpgme_key_unref (key);
		} else {
			g_print(_("Package signed for 0x%s\n"), short_fpr);
		}
	}
}

/**
 * li_pkg_builder_sign_package:
 */
static gchar*
li_pkg_builder_sign_package (LiPkgBuilder *builder, const gchar *tmp_dir, GPtrArray *sign_files, GError **error)
{
	guint i;
	GString *index_str;
	_cleanup_free_ gchar *indexdata = NULL;
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	gpgme_sig_mode_t sigmode = GPGME_SIG_MODE_NORMAL;
	gpgme_data_t din, dout;
	gpgme_sign_result_t sig_res;

	#define BUF_SIZE 512
	gchar *sig_fname;
	char buf[BUF_SIZE + 1];
	FILE *file;
	int ret;
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);

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

	err = gpgme_new (&ctx);
	if (err != 0) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_SIGN,
				_("Signing of package failed: %s"),
				gpgme_strsource (err));
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
					_("Signing of package failed: %s"),
					gpgme_strsource (err));
			return NULL;
		}

		err = gpgme_signers_add (ctx, akey);
		if (err != 0) {
			g_set_error (error,
					LI_BUILDER_ERROR,
					LI_BUILDER_ERROR_SIGN,
					_("Signing of package failed: %s"),
					gpgme_strsource (err));
			return NULL;
		}

		gpgme_key_unref (akey);
	}

	err = gpgme_data_new_from_mem (&din, indexdata, strlen (indexdata), 0);
	if (err != 0) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_SIGN,
				_("Signing of package failed: %s"),
				gpgme_strsource (err));
		return NULL;
	}

	err = gpgme_data_new (&dout);
	if (err != 0) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_SIGN,
				_("Signing of package failed: %s"),
				gpgme_strsource (err));
		return NULL;
	}

	err = gpgme_op_sign (ctx, din, dout, sigmode);
	sig_res = gpgme_op_sign_result (ctx);
	if (sig_res)
		li_print_gpgsign_result (ctx, sig_res, sigmode);

	if (err != 0) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_SIGN,
				_("Signing of package failed: %s"),
				gpgme_strsource (err));
		return NULL;
	}

	sig_fname = g_build_filename (tmp_dir, "_signature", NULL);
	file = fopen (sig_fname, "w");
	if (file == NULL) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_SIGN,
				_("Unable to write signature: %s"),
				g_strerror (errno));
		g_free (sig_fname);
		return NULL;
	}

	gpgme_data_seek (dout, 0, SEEK_SET);
	while ((ret = gpgme_data_read (dout, buf, BUF_SIZE)) > 0)
		fwrite (buf, ret, 1, file);
	fclose (file);

	gpgme_data_release (dout);
	gpgme_data_release (din);
	gpgme_release (ctx);

	return sig_fname;
}

/**
 * li_pkg_builder_create_package_from_dir:
 */
gboolean
li_pkg_builder_create_package_from_dir (LiPkgBuilder *builder, const gchar *dir, const gchar *out_fname, GError **error)
{
	_cleanup_free_ gchar *ctl_fname = NULL;
	_cleanup_free_ gchar *payload_root = NULL;
	_cleanup_free_ gchar *repo_root = NULL;
	_cleanup_free_ gchar *as_metadata = NULL;
	_cleanup_free_ gchar *tmp_dir = NULL;
	_cleanup_free_ gchar *payload_file = NULL;
	_cleanup_free_ gchar *pkg_fname = NULL;
	_cleanup_free_ gchar *sig_fname = NULL;
	_cleanup_object_unref_ GFile *ctlfile = NULL;
	_cleanup_object_unref_ LiPkgInfo *ctl = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *files = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *sign_files = NULL;
	GError *tmp_error = NULL;
	gchar *tmp;
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);

	tmp = g_build_filename (dir, "control", NULL);
	ctlfile = g_file_new_for_path (tmp);
	g_free (tmp);
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

	payload_root = g_build_filename (dir, "inst_target", NULL);
	if (!g_file_test (payload_root, G_FILE_TEST_IS_DIR)) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_NOT_FOUND,
				_("Could not find payload data in the 'inst_target' subdirectory."));
		return FALSE;
	}

	as_metadata = g_build_filename (dir, "metainfo.xml", NULL);
	if (!g_file_test (as_metadata, G_FILE_TEST_IS_REGULAR)) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_NOT_FOUND,
				_("Could not find AppStream metadata for the new package!"));
		return FALSE;
	}

	repo_root = g_build_filename (dir, "repo", NULL);
	if (!g_file_test (repo_root, G_FILE_TEST_IS_DIR)) {
		/* we have no dependency repository */
		g_free (repo_root);
		repo_root = NULL;
	}

	if (out_fname == NULL) {
		AsMetadata *mdata;
		AsComponent *cpt;
		GFile *asfile;
		gchar *tmp;
		const gchar *version;

		/* we need to auto-generate a package filename */
		asfile = g_file_new_for_path (as_metadata);
		if (!g_file_query_exists (asfile, NULL)) {
			g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_FAILED,
				_("Could not generate package filename: AppStream metadata is missing."));
			g_object_unref (asfile);
			return FALSE;
		}

		mdata = as_metadata_new ();
		as_metadata_parse_file (mdata, asfile, &tmp_error);
		cpt = as_metadata_get_component (mdata);
		g_object_unref (asfile);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			g_object_unref (mdata);
			return FALSE;
		}

		tmp = li_str_replace (as_component_get_name (cpt), " ", "");
		version = li_get_last_version_from_component (cpt);
		if (version != NULL)
			pkg_fname = g_strdup_printf ("%s/%s-%s.ipk", dir, tmp, version);
		else
			pkg_fname = g_strdup_printf ("%s/%s.ipk", dir, tmp);
		g_free (tmp);
		g_object_unref (mdata);
	} else {
		pkg_fname = g_strdup (out_fname);
	}

	tmp_dir = li_utils_get_tmp_dir ("build");
	payload_file = g_build_filename (tmp_dir, "main-data.tar.xz", NULL);

	/* create payload */
	li_pkg_builder_write_payload (payload_root, payload_file);

	/* construct package contents */
	files = g_ptr_array_new_with_free_func (g_free);
	sign_files = g_ptr_array_new ();

	if (repo_root != NULL) {
		gchar *repo_fname;
		/* we have extra packages to embed */
		repo_fname = li_pkg_builder_add_embedded_packages (tmp_dir, repo_root, files, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}
		g_ptr_array_add (files, repo_fname);
		/* we need to sign the repo file */
		g_ptr_array_add (sign_files, repo_fname);
	}

	/* handle arch:any notation (resolve to current arch) */
	if (g_strcmp0 (li_pkg_info_get_architecture (ctl), "any") == 0) {
		gchar *arch;
		arch = li_get_current_arch_h ();
		li_pkg_info_set_architecture (ctl, arch);
		g_free (arch);
	}

	/* safe our new control metadata */
	ctl_fname = g_build_filename (tmp_dir, "control", NULL);
	li_pkg_info_save_to_file (ctl, ctl_fname);

	/* we want these files in the package */
	g_ptr_array_add (files, g_strdup (ctl_fname));
	g_ptr_array_add (files, g_strdup (as_metadata));
	g_ptr_array_add (files, g_strdup (payload_file));

	/* these files need to be signed in order to verify the whole package */
	g_ptr_array_add (sign_files, ctl_fname);
	g_ptr_array_add (sign_files, as_metadata);
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
