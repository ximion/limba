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

#include "li-utils.h"
#include "li-utils-private.h"

typedef struct _LiPkgBuilderPrivate	LiPkgBuilderPrivate;
struct _LiPkgBuilderPrivate
{
	gchar *dir;
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
}

/**
 * li_get_package_fname:
 */
static gchar*
li_get_package_fname (const gchar *root_dir, const gchar *disk_fname)
{
	gchar *tmp;
	gchar *fname = NULL;
	const gchar *approot = "/opt/approot";

	if (g_str_has_prefix (disk_fname, root_dir)) {
		fname = g_strdup (disk_fname + strlen (root_dir));
	}
	if (fname != NULL) {
		if (g_str_has_prefix (fname, approot)) {
			tmp = g_strdup (fname);
			g_free (fname);
			fname = g_strdup (tmp + strlen (approot));
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
 * li_pkg_builder_write_package:
 */
static void
li_pkg_builder_write_package (GPtrArray *files, const gchar *out_fname)
{
	struct archive *a;
	struct archive_entry *entry;
	struct stat st;
	char buff[8192];
	int len;
	int fd;
	guint i;

	a = archive_write_new ();
	archive_write_add_filter_none (a);
	archive_write_set_format_pax_restricted (a);
	archive_write_open_filename (a, out_fname);

	for (i = 0; i < files->len; i++) {
		gchar *ar_fname;
		const gchar *fname = (const gchar *) g_ptr_array_index (files, i);

		ar_fname = g_path_get_basename (fname);

		stat(fname, &st);
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

	archive_write_close(a);
	archive_write_free(a);
}

/**
 * li_pkg_builder_create_package_from_dir:
 */
gboolean
li_pkg_builder_create_package_from_dir (LiPkgBuilder *builder, const gchar *dir, const gchar *out_fname, GError **error)
{
	_cleanup_free_ gchar *ctl_fname = NULL;
	_cleanup_free_ gchar *payload_root = NULL;
	_cleanup_free_ gchar *as_metadata = NULL;
	_cleanup_free_ gchar *tmp_dir = NULL;
	_cleanup_free_ gchar *payload_file = NULL;
	_cleanup_free_ gchar *pkg_fname = NULL;
	GPtrArray *files;

	ctl_fname = g_build_filename (dir, "control", NULL);
	if (!g_file_test (ctl_fname, G_FILE_TEST_IS_REGULAR)) {
		g_set_error (error,
				LI_BUILDER_ERROR,
				LI_BUILDER_ERROR_NOT_FOUND,
				_("Could not find control file for the archive!"));
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

	if (out_fname == NULL) {
		AsMetadata *mdata;
		AsComponent *cpt;
		GFile *asfile;
		GError *tmp_error = NULL;
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
		cpt = as_metadata_parse_file (mdata, asfile, &tmp_error);
		g_object_unref (mdata);
		g_object_unref (asfile);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}

		tmp = li_str_replace (as_component_get_name (cpt), " ", "");
		version = li_get_last_version_from_component (cpt);
		if (version != NULL)
			pkg_fname = g_strdup_printf ("%s/%s-%s.ipk", dir, tmp, version);
		else
			pkg_fname = g_strdup_printf ("%s/%s.ipk", dir, tmp);
		g_free (tmp);
		g_object_unref (cpt);
	} else {
		pkg_fname = g_strdup (out_fname);
	}

	tmp_dir = li_utils_get_tmp_dir ("build");
	payload_file = g_build_filename (tmp_dir, "main-data.tar.xz", NULL);

	/* create payload */
	li_pkg_builder_write_payload (payload_root, payload_file);

	/* construct package contents */
	files = g_ptr_array_new ();

	g_ptr_array_add (files, ctl_fname);
	g_ptr_array_add (files, as_metadata);
	g_ptr_array_add (files, payload_file);

	/* write package */
	li_pkg_builder_write_package (files, pkg_fname);

	g_ptr_array_unref (files);

	return TRUE;
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
