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

#include "li-utils-private.h"
#include "li-pkg-index.h"
#include "li-package.h"
#include "li-package-private.h"

typedef struct _LiRepositoryPrivate	LiRepositoryPrivate;
struct _LiRepositoryPrivate
{
	LiPkgIndex *index;
	AsMetadata *metad;
	gchar *repo_path;
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
	g_object_unref (priv->index);
	g_object_unref (priv->metad);

	G_OBJECT_CLASS (li_repository_parent_class)->finalize (object);
}

/**
 * li_repository_init:
 **/
static void
li_repository_init (LiRepository *repo)
{
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	priv->index = li_pkg_index_new ();
	priv->metad = as_metadata_new ();
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

	as_metadata_clear_components (priv->metad);

	/* load index data if we find it */
	fname = g_build_filename (directory, "Index.gz", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);
	if (g_file_query_exists (file, NULL)) {
		li_pkg_index_load_file (priv->index, file, &tmp_error);
	}
	g_object_unref (file);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* load AppStream metadata (if present) */
	fname = g_build_filename (directory, "Metadata.xml", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);
	if (g_file_query_exists (file, NULL)) {
		as_metadata_parse_file (priv->metad, file, &tmp_error);
	}
	g_object_unref (file);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	g_free (priv->repo_path);
	priv->repo_path = g_strdup (directory);

	return TRUE;
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
	gchar *fname;
	gchar *xml;
	GError *tmp_error = NULL;
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	/* ensure the basic directory structure is present */
	dir = g_build_filename (priv->repo_path, "pool", NULL);
	g_mkdir_with_parents (dir, 0755);
	g_free (dir);

	dir = g_build_filename (priv->repo_path, "universe", NULL);
	g_mkdir_with_parents (dir, 0755);
	g_free (dir);

	/* save index */
	fname = g_build_filename (priv->repo_path, "Index.gz", NULL);
	li_pkg_index_save_to_file (priv->index, fname);
	g_free (fname);

	/* save AppStream metadata */
	fname = g_build_filename (priv->repo_path, "Metadata.xml", NULL);
	xml = as_metadata_components_to_distro_xml (priv->metad);
	if (xml != NULL) {
		g_file_set_contents (fname, xml, -1, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
		}
		g_free (xml);
	}
	g_free (fname);

	// TODO: Sign index and AppStream data

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
	gchar *tmp;
	gunichar c;
	guint i;
	_cleanup_free_ gchar *dest_path = NULL;
	gchar **strv;
	AsComponent *cpt;
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
	tmp = g_str_to_ascii (pkgname, NULL);

	for (i = 0; tmp[i] != '\0'; i++) {
		c = tmp[i];
		if (g_ascii_isalnum (c))
			break;
	}
	g_free (tmp);
	c = g_unichar_tolower (c);

	dest_path = g_strdup_printf ("pool/%c/%s-%s.ipk", c, pkgname, pkgversion);
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

	/* now copy the file */
	li_copy_file (pkg_fname, tmp, &tmp_error);
	if (tmp_error != NULL) {
		g_free (tmp);
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* set a unique AppStream package name */
	cpt = li_package_get_appstream_cpt (pkg);
	strv = g_new0 (gchar*, 2);
	strv[0] = g_strdup_printf ("limba::%s", pkgname);
	as_component_set_pkgnames (cpt, strv);
	g_strfreev (strv);

	/* add to indices */
	li_pkg_index_add_package (priv->index, pki);
	as_metadata_add_component (priv->metad, cpt);

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
