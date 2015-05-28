/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2015 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:li-repository
 * @short_description: A local Limba package repository
 *
 */

#include "config.h"
#include "li-repository.h"

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <locale.h>
#include <gpgme.h>
#include <appstream.h>
#include <errno.h>

#include "li-config-data.h"
#include "li-utils-private.h"
#include "li-pkg-index.h"
#include "li-package.h"
#include "li-package-private.h"

typedef struct _LiRepositoryPrivate	LiRepositoryPrivate;
struct _LiRepositoryPrivate
{
	GHashTable *indices;
	GHashTable *asmeta;
	gchar *repo_path;

	LiConfigData *rconfig;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiRepository, li_repository, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_repository_get_instance_private (o))

/**
 * li_repository_finalize:
 **/
static void
li_repository_finalize (GObject *object)
{
	LiRepository *repo = LI_REPOSITORY (object);
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	g_free (priv->repo_path);
	g_hash_table_unref (priv->indices);
	g_hash_table_unref (priv->asmeta);
	g_object_unref (priv->rconfig);

	G_OBJECT_CLASS (li_repository_parent_class)->finalize (object);
}

/**
 * li_repository_init:
 **/
static void
li_repository_init (LiRepository *repo)
{
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	priv->indices = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->asmeta = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->rconfig = li_config_data_new ();
}

/**
 * li_repository_get_index:
 */
static LiPkgIndex*
li_repository_get_index (LiRepository *repo, const gchar *arch)
{
	LiPkgIndex *index;
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	index = g_hash_table_lookup (priv->indices, arch);
	if (index == NULL) {
		index = li_pkg_index_new ();
		g_hash_table_insert (priv->indices, g_strdup (arch), index);
	}

	return index;
}

/**
 * li_repository_get_asmeta:
 */
static AsMetadata*
li_repository_get_asmeta (LiRepository *repo, const gchar *arch)
{
	AsMetadata *metad;
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	metad = g_hash_table_lookup (priv->asmeta, arch);
	if (metad == NULL) {
		metad = as_metadata_new ();
		/* we do not want to filter languages */
		as_metadata_set_locale (metad, "ALL");

		g_hash_table_insert (priv->asmeta, g_strdup (arch), metad);
	}

	return metad;
}

/**
 * li_repository_load_indices:
 */
void
li_repository_load_indices (LiRepository *repo, const gchar* dir, GError **error)
{
	GError *tmp_error = NULL;
	GFileInfo *file_info;
	GFileEnumerator *enumerator = NULL;
	GFile *fdir;
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS))
		return;

	fdir =  g_file_new_for_path (dir);
	enumerator = g_file_enumerate_children (fdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &tmp_error)) != NULL) {
		_cleanup_free_ gchar *path = NULL;

		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

		if (g_file_info_get_is_hidden (file_info))
			continue;
		path = g_build_filename (dir,
								g_file_info_get_name (file_info),
								NULL);

		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			_cleanup_free_ gchar *arch = NULL;
			_cleanup_free_ gchar *fname = NULL;
			GFile *file;

			/* our directory name is the architecture */
			arch = g_path_get_basename (path);

			/* load IPK package index */
			fname = g_build_filename (path, "Index.gz", NULL);
			file = g_file_new_for_path (fname);
			if (g_file_query_exists (file, NULL)) {
				LiPkgIndex *index;

				index = li_pkg_index_new ();
				li_pkg_index_load_file (index, file, &tmp_error);
				if (tmp_error == NULL)
					g_hash_table_insert (priv->indices, g_strdup (arch), index);
			}
			g_object_unref (file);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
			g_free (fname);

			/* load AppStream metadata (if present) */
			fname = g_build_filename (path, "Metadata.xml.gz", NULL);
			file = g_file_new_for_path (fname);
			if (g_file_query_exists (file, NULL)) {
				AsMetadata *metad;

				metad = as_metadata_new ();
				/* we do not want to filter languages */
				as_metadata_set_locale (metad, "ALL");

				as_metadata_parse_file (metad, file, &tmp_error);
				if (tmp_error == NULL)
					g_hash_table_insert (priv->asmeta, g_strdup (arch), metad);
			}
			g_object_unref (file);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}

		}
	}

out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
}

/**
 * li_repository_open:
 */
gboolean
li_repository_open (LiRepository *repo, const gchar *directory, GError **error)
{
	gchar *fname;
	GFile *file;
	GError *tmp_error = NULL;
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	if (!g_file_test (directory, G_FILE_TEST_IS_DIR)) {
		g_set_error (error,
				LI_REPOSITORY_ERROR,
				LI_REPOSITORY_ERROR_FAILED,
				_("Invalid path to directory."));
		return FALSE;
	}

	/* cleanup everything */
	g_hash_table_remove_all (priv->asmeta);
	g_hash_table_remove_all (priv->indices);
	g_object_unref (priv->rconfig);
	priv->rconfig = li_config_data_new ();

	/* load repository configuration */
	fname = g_build_filename (directory, ".repo-config", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);
	if (g_file_query_exists (file, NULL)) {
		li_config_data_load_file (priv->rconfig, file, &tmp_error);
	}
	g_object_unref (file);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* load index data if we find it */
	fname = g_build_filename (directory, "indices", NULL);
	li_repository_load_indices (repo, fname, &tmp_error);
	g_free (fname);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	g_free (priv->repo_path);
	priv->repo_path = g_strdup (directory);

	return TRUE;
}

/**
 * li_repository_sign:
 */
static void
li_repository_sign (LiRepository *repo, const gchar *sigtext, GError **error)
{
	gpgme_error_t err;
	gpgme_ctx_t ctx;
	gpgme_sig_mode_t sigmode = GPGME_SIG_MODE_NORMAL;
	gpgme_data_t din, dout;

	#define BUF_SIZE 512
	gchar *sig_fname;
	char buf[BUF_SIZE + 1];
	FILE *file;
	int ret;
	_cleanup_free_ gchar *gpg_key = NULL;
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	err = gpgme_new (&ctx);
	if (err != 0) {
		g_set_error (error,
				LI_REPOSITORY_ERROR,
				LI_REPOSITORY_ERROR_SIGN,
				_("Signing of repository failed: %s"),
				gpgme_strsource (err));
		return;
	}

	gpg_key = li_config_data_get_value (priv->rconfig, "GPGKey");

	gpgme_set_protocol (ctx, LI_GPG_PROTOCOL);
	gpgme_set_armor (ctx, TRUE);

	if (gpg_key != NULL) {
		gpgme_key_t akey;

		err = gpgme_get_key (ctx, gpg_key, &akey, 1);
		if (err != 0) {
			g_set_error (error,
					LI_REPOSITORY_ERROR,
					LI_REPOSITORY_ERROR_SIGN,
					_("Signing of repository failed: %s"),
					gpgme_strsource (err));
			return;
		}

		err = gpgme_signers_add (ctx, akey);
		if (err != 0) {
			g_set_error (error,
					LI_REPOSITORY_ERROR,
					LI_REPOSITORY_ERROR_SIGN,
					_("Signing of repository failed: %s"),
					gpgme_strsource (err));
			return;
		}

		gpgme_key_unref (akey);
	}

	err = gpgme_data_new_from_mem (&din, sigtext, strlen (sigtext), 0);
	if (err != 0) {
		g_set_error (error,
				LI_REPOSITORY_ERROR,
				LI_REPOSITORY_ERROR_SIGN,
				_("Signing of repository failed: %s"),
				gpgme_strsource (err));
		return;
	}

	err = gpgme_data_new (&dout);
	if (err != 0) {
		g_set_error (error,
				LI_REPOSITORY_ERROR,
				LI_REPOSITORY_ERROR_SIGN,
				_("Signing of repository failed: %s"),
				gpgme_strsource (err));
		return;
	}

	err = gpgme_op_sign (ctx, din, dout, sigmode);

	if (err != 0) {
		g_set_error (error,
				LI_REPOSITORY_ERROR,
				LI_REPOSITORY_ERROR_SIGN,
				_("Signing of repository failed: %s"),
				gpgme_strsource (err));
		return;
	}

	sig_fname = g_build_filename (priv->repo_path, "indices", "Indices.gpg", NULL);
	file = fopen (sig_fname, "w");
	if (file == NULL) {
		g_set_error (error,
				LI_REPOSITORY_ERROR,
				LI_REPOSITORY_ERROR_SIGN,
				_("Unable to write signature: %s"),
				g_strerror (errno));
		g_free (sig_fname);
		return;
	}
	g_free (sig_fname);

	gpgme_data_seek (dout, 0, SEEK_SET);
	while ((ret = gpgme_data_read (dout, buf, BUF_SIZE)) > 0)
		fwrite (buf, ret, 1, file);
	fclose (file);

	gpgme_data_release (dout);
	gpgme_data_release (din);
	gpgme_release (ctx);
}

/**
 * LiIndexSaveHelper:
 * 
 * Temporary helper structure, to make saving and hashing indices easier
 */
typedef struct {
	LiRepository *repo;
	GString *sigtext;

	GError *error;
} LiIndexSaveHelper;

/**
 * li_repository_save_indices:
 *
 * Helper function to save the package-index hashtable on disk
 */
static void
li_repository_save_indices (gchar *arch, LiPkgIndex *index, LiIndexSaveHelper *helper)
{
	_cleanup_free_ gchar *fname = NULL;
	_cleanup_free_ gchar *dir = NULL;
	_cleanup_free_ gchar *checksum = NULL;
	_cleanup_free_ gchar *internal_name = NULL;
	LiRepositoryPrivate *priv = GET_PRIVATE (helper->repo);

	/* don't continue if we already have an error */
	if (helper->error != NULL)
		return;

	dir = g_build_filename (priv->repo_path, "indices", arch, NULL);
	g_mkdir_with_parents (dir, 0755);

	fname = g_build_filename (dir, "Index.gz", NULL);
	li_pkg_index_save_to_file (index, fname);

	if (g_str_has_prefix (fname, priv->repo_path)) {
		internal_name = g_strdup (&fname[strlen (priv->repo_path) + 1]);
	} else {
		internal_name = g_path_get_basename (fname);
	}

	checksum = li_compute_checksum_for_file (fname);
	if (checksum == NULL) {
		g_set_error (&helper->error,
			LI_REPOSITORY_ERROR,
			LI_REPOSITORY_ERROR_SIGN,
			_("Unable to calculate checksum for: %s"),
			internal_name);
		return;
	}

	g_string_append_printf (helper->sigtext, "%s\t%s\n", checksum, internal_name);
}

/**
 * li_repository_save_asmeta:
 *
 * Helper function to save the AppStream metadata hashtable on disk
 */
static void
li_repository_save_asmeta (gchar *arch, AsMetadata *metad, LiIndexSaveHelper *helper)
{
	_cleanup_free_ gchar *fname = NULL;
	_cleanup_free_ gchar *dir = NULL;
	_cleanup_free_ gchar *checksum = NULL;
	_cleanup_free_ gchar *internal_name = NULL;
	LiRepositoryPrivate *priv = GET_PRIVATE (helper->repo);

	/* don't continue if we already have an error */
	if (helper->error != NULL)
		return;

	dir = g_build_filename (priv->repo_path, "indices", arch, NULL);
	g_mkdir_with_parents (dir, 0755);

	fname = g_build_filename (dir, "Metadata.xml.gz", NULL);
	as_metadata_save_distro_xml (metad, fname, &helper->error);
	if (helper->error != NULL)
		return;

	if (g_str_has_prefix (fname, priv->repo_path)) {
		internal_name = g_strdup (&fname[strlen (priv->repo_path) + 1]);
	} else {
		internal_name = g_path_get_basename (fname);
	}

	checksum = li_compute_checksum_for_file (fname);
	if (checksum == NULL) {
		g_set_error (&helper->error,
			LI_REPOSITORY_ERROR,
			LI_REPOSITORY_ERROR_SIGN,
			_("Unable to calculate checksum for: %s"),
			internal_name);
		return;
	}

	g_string_append_printf (helper->sigtext, "%s\t%s\n", checksum, internal_name);
}

/**
 * li_repository_save:
 *
 * Save the repository metadata and sign it.
 */
gboolean
li_repository_save (LiRepository *repo, GError **error)
{
	gchar *dir;
	_cleanup_free_ gchar *fname = NULL;
	GError *tmp_error = NULL;
	LiIndexSaveHelper helper;
	_cleanup_string_free_ GString *sigtext = NULL;
	_cleanup_free_ gchar *internal_name = NULL;
	_cleanup_free_ gchar *checksum = NULL;
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	/* prepare variable to store checksums */
	sigtext = g_string_new ("");

	/* ensure the basic directory structure is present */
	dir = g_build_filename (priv->repo_path, "indices", NULL);
	g_mkdir_with_parents (dir, 0755);
	g_free (dir);

	dir = g_build_filename (priv->repo_path, "assets", NULL);
	g_mkdir_with_parents (dir, 0755);
	g_free (dir);

	dir = g_build_filename (priv->repo_path, "pool", NULL);
	g_mkdir_with_parents (dir, 0755);
	g_free (dir);

	dir = g_build_filename (priv->repo_path, "universe", NULL);
	g_mkdir_with_parents (dir, 0755);
	g_free (dir);

	/* save indices */
	helper.repo = repo;
	helper.sigtext = sigtext;
	helper.error = NULL;

	g_hash_table_foreach (priv->indices,
						(GHFunc) li_repository_save_indices,
						&helper);
	if (helper.error != NULL) {
		g_propagate_error (error, helper.error);
		return FALSE;
	}

	/* save AppStream metadata */
	g_hash_table_foreach (priv->asmeta,
						(GHFunc) li_repository_save_asmeta,
						&helper);
	if (helper.error != NULL) {
		g_propagate_error (error, helper.error);
		return FALSE;
	}

	/* now sign the package */
	li_repository_sign (repo, sigtext->str, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

/**
 * li_repository_add_package:
 */
gboolean
li_repository_add_package (LiRepository *repo, const gchar *pkg_fname, GError **error)
{
	GError *tmp_error = NULL;
	_cleanup_object_unref_ LiPackage *pkg = NULL;
	LiPkgInfo *pki;
	const gchar *pkgname;
	const gchar *pkgversion;
	const gchar *pkgarch;
	gchar *tmp;
	gunichar c;
	guint i;
	_cleanup_free_ gchar *dest_path = NULL;
	_cleanup_free_ gchar *icon_dir = NULL;
	_cleanup_free_ gchar *hash = NULL;
	AsComponent *cpt;
	LiPkgIndex *index;
	AsMetadata *metad;
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	pkg = li_package_new ();
	li_package_open_file (pkg, pkg_fname, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	if (li_package_has_embedded_packages (pkg)) {
		/* We do not support packages with embedded other packages in repositories.
		 * Allowing it would needlessly duplicate data, and would also confuse the
		 * dependency resolver. Embedded dependencies are really just for package-only
		 * distribution.
		 * We can also not just splt out embedded package copies, since that would break
		 * the packages signature and make it invalid.
		 */
		g_set_error (error,
				LI_REPOSITORY_ERROR,
				LI_REPOSITORY_ERROR_EMBEDDED_COPY,
				_("The package contains embedded dependencies. Packages with that property are not allowed in repositories, please add dependencies separately."));
		return FALSE;
	}

	pki = li_package_get_info (pkg);

	/* build destination path and directory */
	pkgname = li_pkg_info_get_name (pki);
	pkgversion = li_pkg_info_get_version (pki);
	pkgarch = li_pkg_info_get_architecture (pki);
	tmp = g_str_to_ascii (pkgname, NULL);

	for (i = 0; tmp[i] != '\0'; i++) {
		c = tmp[i];
		if (g_ascii_isalnum (c))
			break;
	}
	g_free (tmp);
	c = g_unichar_tolower (c);

	dest_path = g_strdup_printf ("pool/%c/%s-%s_%s.ipk",
							c,
							pkgname,
							pkgversion,
							pkgarch);
	tmp = g_strdup_printf ("%s/pool/%c/", priv->repo_path, c);
	g_mkdir_with_parents (tmp, 0755);
	g_free (tmp);

	li_pkg_info_set_repo_location (pki, dest_path);

	/* check if we can copy the file */
	tmp = g_build_filename (priv->repo_path, dest_path, NULL);
	if (g_file_test (tmp, G_FILE_TEST_EXISTS)) {
		g_free (tmp);
		g_set_error (error,
				LI_REPOSITORY_ERROR,
				LI_REPOSITORY_ERROR_FAILED,
				_("A package with the same name and version has already been installed into this repository."));
		return FALSE;
	}

	/* calculate secure checksum to verify the integrity of this package later */
	hash = li_compute_checksum_for_file (pkg_fname);
	li_pkg_info_set_checksum_sha256 (pki, hash);

	/* extract the icons */
	icon_dir = g_build_filename (priv->repo_path,
									"assets",
									li_pkg_info_get_id (pki),
									"icons",
									NULL);
	li_package_extract_appstream_icons (pkg, icon_dir, &tmp_error);
	if (tmp_error != NULL) {
		g_free (tmp);
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* now copy the file */
	li_copy_file (pkg_fname, tmp, &tmp_error);
	if (tmp_error != NULL) {
		g_free (tmp);
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	g_free (tmp);

	/* set a unique AppStream package name */
	cpt = li_package_get_appstream_cpt (pkg);
	as_component_add_bundle_id (cpt, AS_BUNDLE_KIND_LIMBA,
							li_pkg_info_get_id (pki));
	/* remove all package names - just in case */
	as_component_set_pkgnames (cpt, NULL);

	/* add to indices */
	index = li_repository_get_index (repo, pkgarch);
	li_pkg_index_add_package (index, pki);

	metad = li_repository_get_asmeta (repo, pkgarch);
	as_metadata_add_component (metad, cpt);

	return TRUE;
}

/**
 * li_repository_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_repository_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiRepositoryError");
	return quark;
}

/**
 * li_repository_class_init:
 **/
static void
li_repository_class_init (LiRepositoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_repository_finalize;
}

/**
 * li_repository_new:
 *
 * Creates a new #LiRepository.
 *
 * Returns: (transfer full): a #LiRepository
 *
 **/
LiRepository *
li_repository_new (void)
{
	LiRepository *repo;
	repo = g_object_new (LI_TYPE_REPOSITORY, NULL);
	return LI_REPOSITORY (repo);
}
