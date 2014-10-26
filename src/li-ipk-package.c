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
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define DEFAULT_BLOCK_SIZE 65536

typedef struct _LiIPKPackagePrivate	LiIPKPackagePrivate;
struct _LiIPKPackagePrivate
{
	gchar *filename;
	gchar *tmp_dir;
	LiIPKControl *ctl;
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
	if (priv->filename != NULL)
		g_free (priv->filename);

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

	return g_string_free (res, FALSE);;
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

static struct archive*
li_ipk_package_open_base_ipk (LiIPKPackage *ipk, const gchar *filename, GError **error)
{
	struct archive *ar;
	int res;

	/* create new archive object for reading */
	ar = archive_read_new ();
	/* disable compression, as the main tarball is not compressed */
	archive_read_support_filter_none (ar);
	/* ipk bundles are GNU Tarballs */
	archive_read_support_format_tar (ar);

	/* open the file, exit on error */
	res = archive_read_open_filename (ar, filename, (gsize) 4096);
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
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_NOT_FOUND,
				_("Package file '%s' was not found."), filename);
		return FALSE;
	}

	ar = li_ipk_package_open_base_ipk (ipk, filename, &tmp_error);
	if ((ar == NULL) || (tmp_error != NULL)) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	g_free (priv->filename);
	priv->filename = g_strdup (filename);
	/* create our own tmpdir */
	g_free (priv->tmp_dir);
	tmp_str = g_path_get_basename (priv->filename);
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
				archive_read_free (ar);
				return FALSE;
			}
			li_ipk_control_load_data (priv->ctl, ctl_data);
			g_free (ctl_data);
		} else {
			archive_read_data_skip (ar);
		}

	}

	archive_read_close (ar);
	archive_read_free (ar);

	return TRUE;
}

/**
 * li_ipk_package_install:
 */
gboolean
li_ipk_package_install (LiIPKPackage *ipk, GError **error)
{
	struct archive *ar;
	struct archive_entry* e;
	GError *tmp_error = NULL;
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk);

	ar = li_ipk_package_open_base_ipk (ipk, priv->filename, &tmp_error);
	if ((ar == NULL) || (tmp_error != NULL)) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	while (archive_read_next_header (ar, &e) == ARCHIVE_OK) {
		const gchar *pathname;

		pathname = archive_entry_pathname (e);
		if (g_strcmp0 (pathname, "main-data.tar.xz") == 0) {
			li_ipk_package_extract_entry_to (ipk, ar, e, priv->tmp_dir, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				archive_read_free (ar);
				return FALSE;
			}
		} else {
			archive_read_data_skip (ar);
		}
	}

	// TODO

	archive_read_close (ar);
	archive_read_free (ar);
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
