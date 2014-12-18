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
 * SECTION:li-package
 * @short_description: Representation of a complete package
 */

#include "config.h"
#include "li-package.h"

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

#include "li-utils.h"
#include "li-utils-private.h"
#include "li-exporter.h"
#include "li-pkg-index.h"
#include "li-keyring.h"

#define DEFAULT_BLOCK_SIZE 65536

typedef struct _LiPackagePrivate	LiPackagePrivate;
struct _LiPackagePrivate
{
	FILE *archive_file;
	gchar *tmp_dir;
	gchar *tmp_payload_path; /* we cache the extracted payload path for performance reasons */
	LiPkgInfo *info;
	AsComponent *cpt;

	gchar *install_root;
	gchar *id;
	GPtrArray *embedded_packages;

	LiKeyring *kr;
	gchar *signature_data;
	gchar *sig_fpr;
	LiTrustLevel tlevel;
	GHashTable *contents_hash;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPackage, li_package, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_package_get_instance_private (o))

/**
 * li_package_finalize:
 **/
static void
li_package_finalize (GObject *object)
{
	LiPackage *pkg = LI_PACKAGE (object);
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

	g_object_unref (priv->info);
	if (priv->archive_file != NULL)
		fclose (priv->archive_file);
	if (priv->cpt != NULL)
		g_object_unref (priv->cpt);
	if (priv->embedded_packages != NULL)
		g_ptr_array_unref (priv->embedded_packages);
	if (priv->tmp_dir != NULL) {
		li_delete_dir_recursive (priv->tmp_dir);
		g_free (priv->tmp_dir);
	}
	g_free (priv->tmp_payload_path);
	g_free (priv->signature_data);
	g_free (priv->sig_fpr);
	g_free (priv->install_root);
	g_free (priv->id);
	g_object_unref (priv->kr);
	g_hash_table_unref (priv->contents_hash);

	G_OBJECT_CLASS (li_package_parent_class)->finalize (object);
}

/**
 * li_package_init:
 **/
static void
li_package_init (LiPackage *pkg)
{
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

	priv->info = li_pkg_info_new ();
	priv->tmp_dir = NULL;
	priv->archive_file = NULL;
	priv->install_root = g_strdup (LI_SOFTWARE_ROOT);

	priv->kr = li_keyring_new ();
	priv->contents_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	priv->tlevel = LI_TRUST_LEVEL_NONE;
}

/**
 * li_package_read_entry:
 */
static gchar*
li_package_read_entry (LiPackage *pkg, struct archive* ar, GError **error)
{
	const void *buff = NULL;
	gsize size = 0UL;
	off_t offset = {0};
	GString *res;

	res = g_string_new ("");
	while (archive_read_data_block (ar, &buff, &size, &offset) == ARCHIVE_OK) {
		g_string_append_len (res, buff, size);
	}

	return g_string_free (res, FALSE);
}

/**
 * li_package_extract_entry_to:
 */
static gboolean
li_package_extract_entry_to (LiPackage *pkg, struct archive* ar, struct archive_entry* e, const gchar* dest, GError **error)
{
	_cleanup_free_ gchar *fname = NULL;
	const gchar *cstr;
	const gchar *link_target;
	gchar *str;
	gboolean ret;
	gint res;
	gint fd;
	const void *buff = NULL;
	gsize size = 0UL;
	off_t offset = {0};
	off_t output_offset = {0};
	gsize bytes_to_write = 0UL;
	gssize bytes_written = 0L;
	gssize total_written = 0L;
	mode_t filetype;

	filetype = archive_entry_filetype (e);
	if (filetype == S_IFDIR) {
		/* we don't extract directories */
		return TRUE;
	}

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

	/* check if we are dealing with a symlink */
	link_target = archive_entry_symlink (e);
	if ((filetype == S_IFLNK) && (link_target != NULL)) {
		res = symlink (link_target, fname);
	} else {
		link_target = archive_entry_hardlink (e);
		if (link_target != NULL) {
			res = symlink (link_target, fname);
		}
	}
	if (link_target != NULL) {
		if (res != 0) {
			g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_EXTRACT,
				_("Unable to create link. Error: %s"), g_strerror (errno));
			return FALSE;
		}
		ret = TRUE;
		goto done;
	}

	if (filetype != S_IFREG) {
		/* do we really have to extract every type of file?
		 * Symlinks and regular files should be enough for now */
		g_debug ("Skipped extraction of file '%s': No regular file.",
				 archive_entry_pathname (e));
		return TRUE;
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

done:
	/* apply permissions from the archive */
	chmod (fname, archive_entry_mode (e));

	return ret;
}

/**
 * li_package_open_base_ipk:
 **/
static struct archive*
li_package_open_base_ipk (LiPackage *pkg, GError **error)
{
	struct archive *ar;
	int res;
	gchar *magic;
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

	/* create new archive object for reading */
	ar = archive_read_new ();
	/* Limba packages are always GZip compressed... */
	archive_read_support_filter_gzip (ar);
	/* ...and are tarballs */
	archive_read_support_format_tar (ar);

	/* try to read the IPK header (first 8 bytes) */
	rewind (priv->archive_file);
	magic = g_malloc (8);
	fread (magic, sizeof (gchar), 8, priv->archive_file);
	magic[8] = '\0';
	if (g_strcmp0 (magic, LI_IPK_MAGIC) != 0) {
		/* file doesn't have an IPK magic number - try to process it anyway */
		rewind (priv->archive_file);
	}
	g_free (magic);

	/* read the compressed tarball */
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

/**
 * li_package_read_component_data:
 */
static gboolean
li_package_read_component_data (LiPackage *pkg, const gchar *data, GError **error)
{
	AsMetadata *mdata;
	gchar *tmp;
	const gchar *version;
	GError *tmp_error = NULL;
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

	/* ensure we don't leak memory, even if we are using this function wrong... */
	if (priv->cpt != NULL)
		g_object_unref (priv->cpt);

	mdata = as_metadata_new ();
	priv->cpt = as_metadata_parse_data (mdata, data, &tmp_error);
	g_object_unref (mdata);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	tmp = li_str_replace (as_component_get_id (priv->cpt), ".desktop", "");
	g_strstrip (tmp);
	if ((tmp == NULL) || (g_strcmp0 (tmp, "") == 0)) {
		g_free (tmp);
		tmp = li_str_replace (as_component_get_name (priv->cpt), " ", "_");
		if ((tmp == NULL) || (g_strcmp0 (tmp, "") == 0)) {
			g_free (tmp);
			/* no package name is found, we give up */
			g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Could not determine package name."));
			return FALSE;
		}
	}
	li_pkg_info_set_name (priv->info, tmp);
	g_free (tmp);

	/* the human-friendly application name */
	li_pkg_info_set_appname (priv->info, as_component_get_name (priv->cpt));

	version = li_get_last_version_from_component (priv->cpt);
	if (version == NULL) {
		/* no version? give up. */
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Could not determine package version."));
		return FALSE;
	}
	li_pkg_info_set_version (priv->info, version);

	/* now get the package-id */
	li_package_set_id (pkg,
					li_pkg_info_get_id (priv->info));

	return TRUE;
}

/**
 * li_package_open_file:
 */
gboolean
li_package_open_file (LiPackage *pkg, const gchar *filename, GError **error)
{
	struct archive *ar;
	struct archive_entry* e;
	gchar *tmp_str;
	GError *tmp_error = NULL;
	gboolean ret = FALSE;
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

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

	ar = li_package_open_base_ipk (pkg, &tmp_error);
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
			gchar *info_data;
			info_data = li_package_read_entry (pkg, ar, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
			li_pkg_info_load_data (priv->info, info_data);
			/* generate a checksum for this to verify the data later */
			g_hash_table_insert (priv->contents_hash,
						g_strdup ("control"),
						g_compute_checksum_for_string (G_CHECKSUM_SHA256, info_data, -1));
			g_free (info_data);
		} else if (g_strcmp0 (pathname, "metainfo.xml") == 0) {
			gchar *as_data;
			as_data = li_package_read_entry (pkg, ar, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
			li_package_read_component_data (pkg, as_data, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				g_free (as_data);
				goto out;
			}
			/* generate a checksum for this to verify the data later */
			g_hash_table_insert (priv->contents_hash,
						g_strdup ("metainfo.xml"),
						g_compute_checksum_for_string (G_CHECKSUM_SHA256, as_data, -1));
			g_free (as_data);
		} else if (g_strcmp0 (pathname, "repo/index") == 0) {
			LiPkgIndex *idx;
			gchar *index_data;
			index_data = li_package_read_entry (pkg, ar, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
			idx = li_pkg_index_new ();
			li_pkg_index_load_data (idx, index_data);

			/* generate a checksum for this to verify the data later */
			g_hash_table_insert (priv->contents_hash,
						g_strdup ("repo/index"),
						g_compute_checksum_for_string (G_CHECKSUM_SHA256, index_data, -1));
			g_free (index_data);

			/* clean up a possible previous list of packages */
			if (priv->embedded_packages != NULL)
				g_ptr_array_unref (priv->embedded_packages);
			priv->embedded_packages = li_pkg_index_get_packages (idx);

			/* we need to refcount here, otherwise the array will be removed with the index instance in the next step */
			g_ptr_array_ref (priv->embedded_packages);
			g_object_unref (idx);
		} else if (g_strcmp0 (pathname, "_signature") == 0) {
			if (priv->signature_data != NULL)
				g_free (priv->signature_data);
			priv->signature_data = li_package_read_entry (pkg, ar, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
		} else {
			archive_read_data_skip (ar);
		}
	}

	if (priv->cpt == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Invalid package: Component metadata is missing."));
		ret = FALSE;
		goto out;
	}

	ret = TRUE;

out:
	archive_read_close (ar);
	archive_read_free (ar);

	return ret;
}

/**
 * li_package_extract_payload_archive:
 *
 * Returns the temporary path to our extracted payload archive
 */
static const gchar*
li_package_extract_payload_archive (LiPackage *pkg, GError **error)
{
	struct archive *ar;
	struct archive_entry* e1;
	GError *tmp_error = NULL;
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

	if (priv->tmp_payload_path != NULL)
		/* we already extracted the payload, return its path */
		goto finish;

	ar = li_package_open_base_ipk (pkg, &tmp_error);
	if ((ar == NULL) || (tmp_error != NULL)) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	while (archive_read_next_header (ar, &e1) == ARCHIVE_OK) {
		const gchar *pathname;

		pathname = archive_entry_pathname (e1);
		if (g_strcmp0 (pathname, "main-data.tar.xz") == 0) {
			li_package_extract_entry_to (pkg, ar, e1, priv->tmp_dir, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				archive_read_free (ar);
				return NULL;
			}
		} else {
			archive_read_data_skip (ar);
		}
	}

	archive_read_close (ar);
	archive_read_free (ar);

	priv->tmp_payload_path = g_build_filename (priv->tmp_dir, "main-data.tar.xz", NULL);

finish:
	if (!g_file_test (priv->tmp_payload_path, G_FILE_TEST_EXISTS)) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Unable to find or unpack package payload."));
		return NULL;
	}

	return priv->tmp_payload_path;
}

/**
 * li_package_install:
 */
gboolean
li_package_install (LiPackage *pkg, GError **error)
{
	struct archive *payload_ar;
	struct archive_entry* en;
	GError *tmp_error = NULL;
	const gchar *pkg_id = NULL;
	_cleanup_free_ gchar *pkg_root_dir = NULL;
	gchar *tmp;
	gchar *tmp2;
	gint res;
	gboolean ret;
	const gchar *version;
	const gchar *tmp_payload_path;
	_cleanup_object_unref_ LiExporter *exp = NULL;
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

	/* test if we have all data we need for extraction */
	if (priv->cpt == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Could not install package: Component metadata is missing."));
		return FALSE;
	}

	/* add the version number to our control data */
	version = li_pkg_info_get_version (priv->info);
	if (version == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Unable to determine package version."));
		return FALSE;
	}

	/* do we have a valid id? */
	pkg_id = li_package_get_id (pkg);
	if (pkg_id == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Unable to determine a valid package identifier."));
		return FALSE;
	}

	if (!li_pkg_info_matches_current_arch (priv->info)) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_WRONG_ARCHITECTURE,
				_("The package was built for a different architecture."));
		return FALSE;
	}

	if (priv->tlevel < LI_TRUST_LEVEL_LOW) {
		/* we we have a below-low trust level, we either didn't validate yet or validation failed.
		 * in both cases, better validate (again). */
		li_package_verify_signature (pkg, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}
	}

	/* create a new exporter to integrate the new software into the system */
	exp = li_exporter_new ();
	li_exporter_set_pkg_info (exp, priv->info);

	/* the directory where all package data is installed to */
	pkg_root_dir = g_build_filename (priv->install_root, pkg_id, NULL);
	if (g_file_test (pkg_root_dir, G_FILE_TEST_EXISTS)) {
		g_debug ("Package '%s' is already installed, replacing with the new package contents.", pkg_id);
		if (!li_delete_dir_recursive (pkg_root_dir)) {
			g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_EXTRACT,
				_("Unable to remove existing installation of '%s (%s)'."),
						 as_component_get_name (priv->cpt),
						 pkg_id);
				archive_read_free (payload_ar);
			return FALSE;
		}

		/* tell the exporter that it should not file if the to-be-exported files already exist */
		li_exporter_set_override_allowed (exp, TRUE);
	}

	/* extract payload (if necessary) */
	tmp_payload_path = li_package_extract_payload_archive (pkg, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* open the payload archive */
	payload_ar = archive_read_new ();
	/* the payload is an xz compressed tar archive */
	archive_read_support_filter_xz (payload_ar);
	archive_read_support_format_tar (payload_ar);

	/* open the file, exit on error */
	res = archive_read_open_filename (payload_ar, tmp_payload_path, DEFAULT_BLOCK_SIZE);
	if (res != ARCHIVE_OK) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_ARCHIVE,
				_("Could not open IPK payload! Error: %s"), archive_error_string (payload_ar));
		archive_read_free (payload_ar);
		return FALSE;
	}

	/* install payload */
	while (archive_read_next_header (payload_ar, &en) == ARCHIVE_OK) {
		const gchar *filename;
		gchar *path;
		_cleanup_free_ gchar *tmp_str = NULL;
		_cleanup_free_ gchar *dest_path = NULL;
		_cleanup_free_ gchar *dest_fname = NULL;

		filename = archive_entry_pathname (en);
		path = g_path_get_dirname (filename);
		dest_path = g_build_filename (pkg_root_dir, "data", path, NULL);
		g_free (path);

		if (g_mkdir_with_parents (dest_path, 0755) != 0) {
			g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_EXTRACT,
				_("Could not create directory structure '%s'. %s"), dest_path, g_strerror (errno));
				archive_read_free (payload_ar);
				return FALSE;
		}

		li_package_extract_entry_to (pkg, payload_ar, en, dest_path, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			archive_read_free (payload_ar);
			return FALSE;
		}

		tmp_str = g_path_get_basename (archive_entry_pathname (en));
		dest_fname = g_build_filename (dest_path, tmp_str, NULL);

		li_exporter_process_file (exp, archive_entry_pathname (en), dest_fname, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			archive_read_free (payload_ar);
			return FALSE;
		}
	}

	archive_read_free (payload_ar);

	/* install config data */
	tmp = g_build_filename (pkg_root_dir, "control", NULL);
	ret = li_pkg_info_save_to_file (priv->info, tmp);
	g_free (tmp);

	/* install exported index */
	tmp = g_build_filename (pkg_root_dir, "exported", NULL);
	tmp2 = li_exporter_get_exported_files_index (exp);
	g_file_set_contents (tmp, tmp2, -1, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
	}
	g_free (tmp);
	g_free (tmp2);

	return ret;
}

/**
 * li_package_extract_embedded_package:
 *
 * Return: (transfer full): A new package
 */
LiPackage*
li_package_extract_embedded_package (LiPackage *pkg, LiPkgInfo *pki, GError **error)
{
	struct archive *ar;
	struct archive_entry* en;
	gchar *fname;
	gchar *hash;
	_cleanup_free_ gchar *pkg_basename = NULL;
	LiPackage *subpkg;
	GError *tmp_error = NULL;
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

	ar = li_package_open_base_ipk (pkg, &tmp_error);
	if ((ar == NULL) || (tmp_error != NULL)) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	/* create expected filename */
	pkg_basename = g_strdup_printf ("%s-%s.ipk", li_pkg_info_get_name (pki), li_pkg_info_get_version (pki));
	fname = g_build_filename ("repo", pkg_basename, NULL);

	while (archive_read_next_header (ar, &en) == ARCHIVE_OK) {
		const gchar *pathname;

		pathname = archive_entry_pathname (en);
		if (g_strcmp0 (pathname, fname) == 0) {
			li_package_extract_entry_to (pkg, ar, en, priv->tmp_dir, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				archive_read_free (ar);
				g_free (fname);
				return NULL;
			}
		} else {
			archive_read_data_skip (ar);
		}
	}

	g_free (fname);
	archive_read_close (ar);
	archive_read_free (ar);

	fname = g_build_filename (priv->tmp_dir, pkg_basename, NULL);
	if (!g_file_test (fname, G_FILE_TEST_IS_REGULAR)) {
		g_set_error (error,
			LI_PACKAGE_ERROR,
			LI_PACKAGE_ERROR_NOT_FOUND,
				_("Embedded package '%s' was not found."), li_pkg_info_get_name (pki));
		g_free (fname);
		return NULL;
	}

	hash = li_compute_checksum_for_file (fname);
	if (g_strcmp0 (hash, li_pkg_info_get_checksum_sha256 (pki)) != 0) {
		g_set_error (error,
			LI_PACKAGE_ERROR,
			LI_PACKAGE_ERROR_CHECKSUM_MISMATCH,
				_("Checksum for embedded package '%s' did not match."), li_pkg_info_get_name (pki));
				g_free (fname);
				g_free (hash);
				return NULL;
	}
	g_free (hash);

	subpkg = li_package_new ();
	li_package_open_file (subpkg, fname, &tmp_error);
	g_free (fname);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_object_unref (subpkg);
		return NULL;
	}

	return subpkg;
}

/**
 * li_package_extract_contents:
 *
 * Extracts the package contants into a directory.
 * Useful to inspect the package structure manually.
 */
void
li_package_extract_contents (LiPackage *pkg, const gchar *dest_dir, GError **error)
{
	struct archive *ar;
	struct archive_entry* e;
	GError *tmp_error = NULL;
	g_assert_nonnull (dest_dir);

	ar = li_package_open_base_ipk (pkg, &tmp_error);
	if ((ar == NULL) || (tmp_error != NULL)) {
		g_propagate_error (error, tmp_error);
		return;
	}

	while (archive_read_next_header (ar, &e) == ARCHIVE_OK) {
		gchar *tmp;
		gchar *dest;

		tmp = g_path_get_dirname (archive_entry_pathname (e));
		dest = g_build_filename (dest_dir,  tmp, NULL);
		g_free (tmp);
		g_mkdir_with_parents (dest, 0755);

		li_package_extract_entry_to (pkg, ar, e, dest, &tmp_error);
		g_free (dest);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}
	}

out:
	archive_read_close (ar);
	archive_read_free (ar);
}

/**
 * _li_package_signature_hash_matches:
 *
 * Helper function for verify_signature()
 */
gboolean
_li_package_signature_hash_matches (GHashTable *contents_hash, gchar **sigparts, const gchar *fname)
{
	gint i;
	gboolean valid;
	_cleanup_free_ gchar *hash = NULL;

	/* if we don't have that file which needs to be checked, we consider this as a match */
	if (g_hash_table_lookup (contents_hash, fname) == NULL)
		return TRUE;

	for (i = 0; sigparts[i] != NULL; i++) {
		if (g_str_has_suffix (sigparts[i], fname)) {
			gchar **tmp;
			tmp = g_strsplit (sigparts[i], "\t", 2);
			if (g_strv_length (tmp) != 2) {
				g_strfreev (tmp);
				continue;
			}
			hash = g_strdup (tmp[0]);
			g_strfreev (tmp);
			break;
		}
	}

	valid = g_strcmp0 (g_hash_table_lookup (contents_hash, fname), hash) == 0;
	if (!valid)
		g_debug ("Hash values on IPK metadata '%s' do not match the signature.", fname);
	return valid;
}

/**
 * li_package_verify_signature:
 *
 * Verifies the signature of this package and returns a trust level.
 */
LiTrustLevel
li_package_verify_signature (LiPackage *pkg, GError **error)
{
	GError *tmp_error = NULL;
	const gchar *payload_fname;
	_cleanup_free_ gchar *sig_content = NULL;
	LiTrustLevel level;
	gchar **parts = NULL;
	LiPackagePrivate *priv = GET_PRIVATE (pkg);

	priv->tlevel = LI_TRUST_LEVEL_NONE;

	/* no signature == no trust level */
	if (priv->signature_data == NULL)
		return priv->tlevel;

	/* we need a hash of the payload archive. That value is not automatically generated, since
	 * the payload might be huge and generating the hash might take some time. In case the LiPackage
	 * is just used to peek some metadata (e.g. the AppStream XML), the hashing process would take time
	 * for no gain. So we do it here, when we actually need it. */
	if (g_hash_table_lookup (priv->contents_hash, "main-data.tar.xz") == NULL) {
		payload_fname = li_package_extract_payload_archive (pkg, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return priv->tlevel;
		}
		g_hash_table_insert (priv->contents_hash,
						g_strdup ("main-data.tar.xz"),
						li_compute_checksum_for_file (payload_fname));
	}

	priv->tlevel = LI_TRUST_LEVEL_INVALID;
	if (priv->sig_fpr != NULL)
		g_free (priv->sig_fpr);

	level = li_keyring_process_pkg_signature (priv->kr, priv->signature_data, &sig_content, &priv->sig_fpr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	/* if we are here, we need to validate that all hashsums match */
	/* we check them explicitly, to prevent an attack where the attacker could just provide an empty signature */
	parts = g_strsplit (sig_content, "\n", -1);

	if (!_li_package_signature_hash_matches (priv->contents_hash, parts, "control"))
		goto invalid_error;
	if (!_li_package_signature_hash_matches (priv->contents_hash, parts, "metainfo.xml"))
		goto invalid_error;
	if (!_li_package_signature_hash_matches (priv->contents_hash, parts, "main-data.tar.xz"))
		goto invalid_error;
	if (!_li_package_signature_hash_matches (priv->contents_hash, parts, "repo/index"))
		goto invalid_error;

	/* everything was okay, assign the signature trust level */
	priv->tlevel = level;

invalid_error:
	if (priv->tlevel == LI_TRUST_LEVEL_INVALID) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_SIGNATURE_BROKEN,
				_("This package has a broken signature."));
	}

out:
	if (parts != NULL)
		g_strfreev (parts);

	return priv->tlevel;
}

/**
 * li_package_get_install_root:
 *
 * Get the installation root directory
 */
const gchar*
li_package_get_install_root (LiPackage *pkg)
{
	LiPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->install_root;
}

/**
 * li_package_set_install_root:
 * @dir: An absolute path to the installation root directory
 *
 * Set the directory where the software should be installed to.
 */
void
li_package_set_install_root (LiPackage *pkg, const gchar *dir)
{
	LiPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->install_root);
	priv->install_root = g_strdup (dir);
}

/**
 * li_package_get_id:
 *
 * Get the unique name of this package
 */
const gchar*
li_package_get_id (LiPackage *pkg)
{
	LiPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->id;
}

/**
 * li_package_set_id:
 * @unique_name: A unique name, build from the package name and version
 *
 * Se the unique name for this package.
 */
void
li_package_set_id (LiPackage *pkg, const gchar *unique_name)
{
	LiPackagePrivate *priv = GET_PRIVATE (pkg);
	g_free (priv->id);
	priv->id = g_strdup (unique_name);
}

/**
 * li_package_get_info:
 *
 * Get the archive control metadata object.
 *
 * Returns: (transfer none): An instance of #LiPkgInfo
 */
LiPkgInfo*
li_package_get_info (LiPackage *pkg)
{
	LiPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->info;
}

/**
 * li_package_get_embedded_packages:
 *
 * Returns: (transfer none) (element-type LiPkgInfo): Array of available embedded packages
 */
GPtrArray*
li_package_get_embedded_packages (LiPackage *pkg)
{
	LiPackagePrivate *priv = GET_PRIVATE (pkg);
	return priv->embedded_packages;
}

/**
 * li_package_get_appstream_data:
 *
 * Returns AppStream XML data
 */
gchar*
li_package_get_appstream_data (LiPackage *pkg)
{
	LiPackagePrivate *priv = GET_PRIVATE (pkg);
	if (priv->cpt == NULL)
		return NULL;
	return as_component_to_xml (priv->cpt);
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
 * li_package_class_init:
 **/
static void
li_package_class_init (LiPackageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_package_finalize;
}

/**
 * li_package_new:
 *
 * Creates a new #LiPackage.
 *
 * Returns: (transfer full): a #LiPackage
 *
 **/
LiPackage *
li_package_new (void)
{
	LiPackage *pkg;
	pkg = g_object_new (LI_TYPE_PACKAGE, NULL);
	return LI_PACKAGE (pkg);
}
