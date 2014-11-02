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
 * SECTION:li-polylinker
 * @short_description: Link software components together to form a framework (environment) to run applications in.
 *
 * NOTE: The name of this class is just a placeholder, until it's clearly defined what a "framework" and
 * "environment" actually means this context.
 */

#include "config.h"
#include "li-polylinker.h"

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <errno.h>

#include "li-config.h"
#include "li-pkg-info.h"
#include "li-manager.h"
#include "li-utils-private.h"

typedef struct _LiPolylinkerPrivate	LiPolylinkerPrivate;
struct _LiPolylinkerPrivate
{
	guint dummy;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPolylinker, li_polylinker, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_polylinker_get_instance_private (o))

/**
 * li_polylinker_finalize:
 **/
static void
li_polylinker_finalize (GObject *object)
{
#if 0
	LiPolylinker *plink = LI_POLYLINKER (object);
	LiPolylinkerPrivate *priv = GET_PRIVATE (plink);
#endif

	G_OBJECT_CLASS (li_polylinker_parent_class)->finalize (object);
}

/**
 * li_polylinker_init:
 **/
static void
li_polylinker_init (LiPolylinker *plink)
{
}

/**
 * li_utils_find_files_matching:
 */
static gboolean
li_polylinker_link_software (const gchar* sw_dir, const gchar* frmw_destination, GError **error)
{
	GError *tmp_error = NULL;
	GFileInfo *file_info;
	GFileEnumerator *enumerator = NULL;
	GFile *fdir;

	fdir =  g_file_new_for_path (sw_dir);
	enumerator = g_file_enumerate_children (fdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &tmp_error);
	if (tmp_error != NULL)
		goto out;

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &tmp_error)) != NULL) {
		gchar *path;
		gchar *tmp;
		gchar *dest_path;
		gint res;

		path = g_build_filename (sw_dir,
								 g_file_info_get_name (file_info),
								 NULL);
		tmp = g_path_get_basename (path);
		dest_path = g_build_filename (frmw_destination, tmp, NULL);
		g_free (tmp);

		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			/* if there is already a file with that name at the destination, we skip this directory */
			if (g_file_test (dest_path, G_FILE_TEST_IS_REGULAR)) {
				g_free (path);
				g_free (dest_path);
				continue;
			}

			/* create directory at destination */
			if (!g_file_test (dest_path, G_FILE_TEST_IS_DIR)) {
				res = g_mkdir (dest_path, 0755);
				if (res != 0) {
					g_set_error (error,
						G_FILE_ERROR,
						G_FILE_ERROR_FAILED,
						_("Unable to create directory. Error: %s"), g_strerror (errno));
					goto out;
				}
			}

			li_polylinker_link_software (path, dest_path, &tmp_error);
			/* if there was an error, exit */
			if (tmp_error != NULL) {
				g_free (path);
				g_free (dest_path);
				goto out;
			}
		} else {
			if (g_file_test (path, G_FILE_TEST_IS_SYMLINK)) {
				gchar *target;
				target = g_file_read_link (path, &tmp_error);
				if (tmp_error != NULL)
					goto out;
				res = symlink (target, dest_path);
				g_free (target);
			} else {
				/* we have a file, link it */
				res = link (path, dest_path);
			}
			if (res != 0) {
				g_set_error (error,
					G_FILE_ERROR,
					G_FILE_ERROR_FAILED,
					_("Unable to create symbolic link. Error: %s"), g_strerror (errno));
				goto out;
			}
		}

		g_free (path);
		g_free (dest_path);
	}
	if (tmp_error != NULL)
		goto out;

out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

/**
 * li_polylinker_get_framework_for:
 * @sw: A list of software as #LiPkgInfo
 *
 * Get the ID of a framework which provides the software mentioned
 * in the sw array.
 * In case it doesn't exist yet, generate it.
 */
gchar*
li_polylinker_get_framework_for (LiPolylinker *plink, GPtrArray *sw, GError **error)
{
	guint i;
	gchar *uuid;
	gchar *frmw_path;
	gboolean ret = TRUE;
	GError *tmp_error = NULL;
//	LiPolylinkerPrivate *priv = GET_PRIVATE (plink);

	// TODO: Get list of frameworks and check if one already exists before creating a new one

	uuid = li_get_uuid_string ();
	frmw_path = g_build_filename (LI_INSTALL_ROOT, "tmp", uuid, "data", NULL);

	li_touch_dir (frmw_path, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	for (i = 0; i < sw->len; i++) {
		gchar *data_path;
		const gchar *pkid;
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (sw, i));

		pkid = li_pkg_info_get_id (pki);
		if (pkid == NULL) {
			g_warning ("Found package without identifier!");
			continue;
		}

		data_path = g_build_filename (LI_INSTALL_ROOT, pkid, "data", NULL);
		li_polylinker_link_software (data_path, frmw_path, &tmp_error);
		g_free (data_path);

		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}
	}

out:
	g_free (frmw_path);
	if (ret) {
		return uuid;
	} else {
		g_free (uuid);
		return NULL;
	}
}

/**
 * li_polylinker_class_init:
 **/
static void
li_polylinker_class_init (LiPolylinkerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_polylinker_finalize;
}

/**
 * li_polylinker_new:
 *
 * Creates a new #LiPolylinker.
 *
 * Returns: (transfer full): a #LiPolylinker
 *
 **/
LiPolylinker *
li_polylinker_new (void)
{
	LiPolylinker *plink;
	plink = g_object_new (LI_TYPE_POLYLINKER, NULL);
	return LI_POLYLINKER (plink);
}
