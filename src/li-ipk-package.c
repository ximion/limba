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
 * SECTION:li-ipk-package
 * @short_description: Representation of a complete Listaller package
 */

#include "config.h"
#include "li-ipk-package.h"

#include "limba.h"
#include "li-utils-private.h"

#include <glib/gi18n-lib.h>
#include <archive_entry.h>
#include <archive.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <appstream.h>

#define DEFAULT_BLOCK_SIZE 65536

typedef struct _LiIPKPackagePrivate	LiIPKPackagePrivate;
struct _LiIPKPackagePrivate
{
	FILE *archive_file;
	gchar *tmp_dir;
	LiIPKControl *ctl;
	AsComponent *cpt;
	gchar *install_root;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiIPKPackage, li_ipk_package, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_ipk_package_get_instance_private (o))

/**
 * li_ipk_package_finalize:
 **/
static void
li_ipk_package_finalize (GObject *object)
{
	LiIPKPackage *ipk = LI_IPK_PACKAGE (object);
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	g_object_unref (priv->ctl);
	if (priv->tmp_dir != NULL)
		g_free (priv->tmp_dir);
	if (priv->archive_file != NULL)
		fclose (priv->archive_file);
	if (priv->cpt != NULL)
		g_object_unref (priv->cpt);
	g_free (priv->install_root);

	G_OBJECT_CLASS (li_ipk_package_parent_class)->finalize (object);
}

/**
 * li_ipk_package_init:
 **/
static void
li_ipk_package_init (LiIPKPackage *ipk)
{
	gchar *template;
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	priv->ctl = li_ipk_control_new ();
	priv->tmp_dir = NULL;
	priv->archive_file = NULL;
	priv->install_root = g_strdup (LI_INSTALL_ROOT);
}

/**
 * li_ipk_package_read_entry:
 */
static gchar*
li_ipk_package_read_entry (LiIPKPackage *ipk, struct archive* ar, GError **error)
{
	const void *buff = NULL;
	gsize size = 0UL;
	off_t offset = {0};
	off_t output_offset = {0};
	gsize bytes_to_write = 0UL;
	GString *res;

	res = g_string_new ("");
	while (archive_read_data_block (ar, &buff, &size, &offset) == ARCHIVE_OK) {
		g_string_append_len (res, buff, size);
	}

	return g_string_free (res, FALSE);
}

/**
 * li_ipk_package_extract_entry_to:
 */
static gboolean
li_ipk_package_extract_entry_to (LiIPKPackage *ipk, struct archive* ar, struct archive_entry* e, const gchar* dest, GError **error)
{
	_cleanup_free_ gchar *fname;
	const gchar *cstr;
	gchar *str;
	gboolean ret;
	gint fd;
	const void *buff = NULL;
	gsize size = 0UL;
	off_t offset = {0};
	off_t output_offset = {0};
	gsize bytes_to_write = 0UL;
	gssize bytes_written = 0L;
	gssize total_written = 0L;

	cstr = archive_entry_pathname (e);
	str = g_path_get_basename (cstr);
	fname = g_build_filename (dest, str, NULL);
	g_free (str);

	if (g_file_test (fname, G_FILE_TEST_EXISTS)) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_OVERRIDE,
				_("Could not override file '%s'. The file already exists!"), fname);
		return FALSE;
	}

	fd = open (fname, (O_CREAT | O_WRONLY) | O_TRUNC, ((S_IRUSR | S_IWUSR) | S_IRGRP) | S_IROTH);
	if (fd < 0) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_EXTRACT,
				_("Unable to extract file. Error: %s"), g_strerror (errno));
		return FALSE;
	}

	ret = TRUE;
	while (archive_read_data_block (ar, &buff, &size, &offset) == ARCHIVE_OK) {
		if (offset > output_offset) {
			lseek (fd, offset - output_offset, SEEK_CUR);
			output_offset = offset;
		}
		while (size > (gssize) 0) {
			bytes_to_write = size;
			if (bytes_to_write > DEFAULT_BLOCK_SIZE)
				bytes_to_write = DEFAULT_BLOCK_SIZE;

			bytes_written = write (fd, buff, bytes_to_write);
			if (bytes_written < ((gssize) 0)) {
				g_set_error (error,
					LI_PACKAGE_ERROR,
					LI_PACKAGE_ERROR_EXTRACT,
					_("Unable to extract file. Error: %s"), g_strerror (errno));
				ret = FALSE;
				break;
			}
			output_offset += bytes_written;
			total_written += bytes_written;
			buff += bytes_written;
			size -= bytes_written;
		}
		if (!ret)
			break;
	}

	if (close (fd) != 0) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_FAILED,
				_("Closing of file desriptor failed. Error: %s"), g_strerror (errno));
		return FALSE;
	}

	/* apply permissions from the archive */
	chmod (fname, archive_entry_mode (e));

	return ret;
}

/**
 * li_ipk_package_open_base_ipk:
 **/
static struct archive*
li_ipk_package_open_base_ipk (LiIPKPackage *ipk, GError **error)
{
	struct archive *ar;
	int res;
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	/* create new archive object for reading */
	ar = archive_read_new ();
	/* disable compression, as the main tarball is not compressed */
	archive_read_support_filter_none (ar);
	/* ipk bundles are GNU Tarballs */
	archive_read_support_format_tar (ar);

	/* open the file, exit on error */
	rewind (priv->archive_file);
	res = archive_read_open_FILE (ar, priv->archive_file);
	if (res != ARCHIVE_OK) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_ARCHIVE,
				_("Could not open IPK file! Error: %s"), archive_error_string (ar));
		archive_read_free (ar);
		return NULL;
	}

	return ar;
}

static void
li_ipk_package_read_component_data (LiIPKPackage *ipk, const gchar *data, GError **error)
{
	AsMetadata *mdata;
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	/* ensure we don't leak memory, even if we are using this function wrong... */
	if (priv->cpt != NULL)
		g_object_unref (priv->cpt);

	mdata = as_metadata_new ();
	priv->cpt = as_metadata_parse_data (mdata, data, error);
	g_object_unref (mdata);
}

/**
 * li_ipk_package_open_file:
 */
gboolean
li_ipk_package_open_file (LiIPKPackage *ipk, const gchar *filename, GError **error)
{
	struct archive *ar;
	struct archive_entry* e;
	gchar *tmp_str;
	GError *tmp_error = NULL;
	gboolean ret = FALSE;
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_NOT_FOUND,
				_("Package file '%s' was not found."), filename);
		return FALSE;
	}

	priv->archive_file = fopen (filename, "r");
	if (priv->archive_file == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_ARCHIVE,
				_("Could not open IPK file! Error: %s"), g_strerror (errno));
		return FALSE;
	}

	ar = li_ipk_package_open_base_ipk (ipk, &tmp_error);
	if ((ar == NULL) || (tmp_error != NULL)) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* create our own tmpdir */
	g_free (priv->tmp_dir);
	tmp_str = g_path_get_basename (filename);
	priv->tmp_dir = li_utils_get_tmp_dir (tmp_str);
	g_free (tmp_str);

	while (archive_read_next_header (ar, &e) == ARCHIVE_OK) {
		const gchar *pathname;

		pathname = archive_entry_pathname (e);
		if (g_strcmp0 (pathname, "control") == 0) {
			gchar *ctl_data;
			ctl_data = li_ipk_package_read_entry (ipk, ar, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
			li_ipk_control_load_data (priv->ctl, ctl_data);
			g_free (ctl_data);
		} else if (g_strcmp0 (pathname, "metainfo.xml") == 0) {
			gchar *as_data;
			as_data = li_ipk_package_read_entry (ipk, ar, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
			li_ipk_package_read_component_data (ipk, as_data, &tmp_error);
			g_free (as_data);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
		} else {
			archive_read_data_skip (ar);
		}
	}

	ret = TRUE;

out:
	archive_read_close (ar);
	archive_read_free (ar);

	return ret;
}

static const gchar*
li_ipk_package_get_version_from_component (AsComponent *cpt)
{
	GPtrArray *releases;
	AsRelease *release = NULL;
	guint64 timestamp = 0;
	guint i;
	const gchar *version = NULL;

	releases = as_component_get_releases (cpt);
	for (i = 0; i < releases->len; i++) {
		AsRelease *r = AS_RELEASE (g_ptr_array_index (releases, i));
		if (as_release_get_timestamp (r) >= timestamp) {
				release = r;
				timestamp = as_release_get_timestamp (r);
		}
	}
	if (release != NULL) {
		version = as_release_get_version (release);
	}

	return version;
}

/**
 * li_ipk_package_build_package_id:
 * Create a unique identifier for this software.
 */
static gchar*
li_ipk_package_build_package_id (LiIPKPackage *ipk)
{
	gchar *tmp;
	GString *uname;
	AsComponent *cpt;
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	cpt = priv->cpt;
	uname = g_string_new ("");
	tmp = li_str_replace (as_component_get_id (cpt), ".desktop", "");
	g_strstrip (tmp);
	if ((tmp == NULL) || (g_strcmp0 (tmp, "") == 0)) {
		g_free (tmp);
		tmp = li_str_replace (as_component_get_name (cpt), " ", "_");
		if ((tmp == NULL) || (g_strcmp0 (tmp, "") == 0)) {
			g_free (tmp);
			/* no unique name is possible, we give up */
			return NULL;
		}
	}
	g_string_append (uname, tmp);
	g_string_append_printf (uname, "-%s", li_ipk_control_get_pkg_version (priv->ctl));

	return g_string_free (uname, FALSE);
}

/**
 * li_ipk_package_install:
 */
gboolean
li_ipk_package_install (LiIPKPackage *ipk, GError **error)
{
	struct archive *ar;
	struct archive_entry* e1;
	struct archive *payload_ar;
	struct archive_entry* en;
	GError *tmp_error = NULL;
	_cleanup_free_ gchar *pkg_name = NULL;
	_cleanup_free_ gchar *pkg_root_dir = NULL;
	gchar *tmp;
	gint res;
	gboolean ret;
	const gchar *version;
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	/* test if we have all data we need for extraction */
	if (priv->cpt == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Could not install package: Component metadata is missing."));
		return FALSE;
	}

	/* add the version number to our control data */
	version = li_ipk_package_get_version_from_component (priv->cpt);
	if (version == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Unable to determine package version."));
		return FALSE;
	}
	li_ipk_control_set_pkg_version (priv->ctl, version);

	/* do we have a valid id? */
	pkg_name = li_ipk_package_build_package_id (ipk);
	if (pkg_name == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Unable to determine a valid package name."));
		return FALSE;
	}

	/* the directory where all package data is installed to */
	pkg_root_dir = g_build_filename (priv->install_root, pkg_name, NULL);

	ar = li_ipk_package_open_base_ipk (ipk, &tmp_error);
	if ((ar == NULL) || (tmp_error != NULL)) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	while (archive_read_next_header (ar, &e1) == ARCHIVE_OK) {
		const gchar *pathname;

		pathname = archive_entry_pathname (e1);
		if (g_strcmp0 (pathname, "main-data.tar.xz") == 0) {
			li_ipk_package_extract_entry_to (ipk, ar, e1, priv->tmp_dir, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				archive_read_free (ar);
				return FALSE;
			}
		} else {
			archive_read_data_skip (ar);
		}
	}

	archive_read_close (ar);
	archive_read_free (ar);

	/* open the payload archive */
	payload_ar = archive_read_new ();
	archive_read_support_filter_xz (payload_ar);
	archive_read_support_format_tar (payload_ar);

	tmp = g_build_filename (priv->tmp_dir, "main-data.tar.xz", NULL);
	if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Unable to find or unpack package payload."));
		g_free (tmp);
		archive_read_free (payload_ar);
		return FALSE;
	}

	/* open the file, exit on error */
	res = archive_read_open_filename (payload_ar, tmp, DEFAULT_BLOCK_SIZE);
	g_free (tmp);
	if (res != ARCHIVE_OK) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_ARCHIVE,
				_("Could not open IPK payload! Error: %s"), archive_error_string (ar));
		archive_read_free (payload_ar);
		return FALSE;
	}

	/* install payload */
	while (archive_read_next_header (payload_ar, &en) == ARCHIVE_OK) {
		const gchar *filename;
		gchar *path;
		_cleanup_free_ gchar *dest_path;

		filename = archive_entry_pathname (en);
		path = g_path_get_dirname (filename);
		dest_path = g_build_filename (pkg_root_dir, "data", path, NULL);
		g_free (path);

		if (!li_utils_touch_dir (dest_path)) {
			g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_EXTRACT,
				_("Could not create directory structure '%s'."), dest_path);
				archive_read_free (payload_ar);
				return FALSE;
		}

		li_ipk_package_extract_entry_to (ipk, payload_ar, en, dest_path, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			archive_read_free (ar);
			return FALSE;
		}
	}

	archive_read_free (payload_ar);

	return TRUE;
}

/**
 * li_ipk_package_get_install_root:
 *
 * Get the installation root directory
 */
const gchar*
li_ipk_package_get_install_root (LiIPKPackage *ipk)
{
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);
	return priv->install_root;
}

/**
 * li_ipk_package_set_install_root:
 * @dir: An absolute path to the installation root directory
 *
 * Set the directory where the software should be installed to.
 */
void
li_ipk_package_set_install_root (LiIPKPackage *ipk, const gchar *dir)
{
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);
	g_free (priv->install_root);
	priv->install_root = g_strdup (dir);
}

/**
 * li_package_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_package_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiPackageError");
	return quark;
}

/**
 * li_ipk_package_class_init:
 **/
static void
li_ipk_package_class_init (LiIPKPackageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_ipk_package_finalize;
}

/**
 * li_ipk_package_new:
 *
 * Creates a new #LiIPKPackage.
 *
 * Returns: (transfer full): a #LiIPKPackage
 *
 **/
LiIPKPackage *
li_ipk_package_new (void)
{
	LiIPKPackage *ipk;
	ipk = g_object_new (LI_TYPE_IPK_PACKAGE, NULL);
	return LI_IPK_PACKAGE (ipk);
}
