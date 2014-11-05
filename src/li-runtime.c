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
 * SECTION:li-runtime
 * @short_description: Control metadata for temporary runtime environments
 */

#include "config.h"
#include "li-runtime.h"

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "li-utils.h"
#include "li-utils-private.h"
#include "li-config-data.h"
#include "li-pkg-info.h"

typedef struct _LiRuntimePrivate	LiRuntimePrivate;
struct _LiRuntimePrivate
{
	gchar *dir;
	gchar *uuid; /* auto-generated */
	GPtrArray *members;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiRuntime, li_runtime, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_runtime_get_instance_private (o))

/**
 * li_runtime_fetch_values_from_cdata:
 **/
static void
li_runtime_fetch_values_from_cdata (LiRuntime *rt, LiConfigData *cdata)
{
	gchar *tmp;
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	g_ptr_array_remove_range (priv->members, 0, priv->members->len);
	tmp = li_config_data_get_value (cdata, "Members");
	if (tmp != NULL) {
		guint i;
		gchar **strv;
		strv = g_strsplit (tmp, ",", -1);

		for (i = 0; strv[i] != NULL; i++) {
			g_strstrip (strv[i]);
			g_ptr_array_add (priv->members, g_strdup (strv[1]));
		}
	}
	g_free (tmp);
}

#if 0
/**
 * li_runtime_update_cdata_values:
 **/
static void
li_runtime_update_cdata_values (LiRuntime *rt, LiConfigData *cdata)
{
	guint i;
	GString *str;
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	str = g_string_new ("");
	for (i=0; i < priv->members->len; i++) {
		const gchar *value;
		value = (const gchar *) g_ptr_array_index (priv->members, i);
		g_string_append_printf (str, "%s, ", value);
	}
	if (str->len > 0) {
		g_string_truncate (str, str->len - 2);
		li_config_data_set_value (cdata, "Members", str->str);
	}
	g_string_free (str, TRUE);

}
#endif

/**
 * li_runtime_finalize:
 **/
static void
li_runtime_finalize (GObject *object)
{
	LiRuntime *rt = LI_RUNTIME (object);
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	g_free (priv->uuid);
	g_free (priv->dir);
	g_ptr_array_unref (priv->members);

	G_OBJECT_CLASS (li_runtime_parent_class)->finalize (object);
}

/**
 * li_runtime_init:
 **/
static void
li_runtime_init (LiRuntime *rt)
{
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	priv->dir = NULL;
	priv->uuid = li_get_uuid_string ();
	priv->members = g_ptr_array_new_with_free_func (g_free);
}

/**
 * li_runtime_load_directory:
 */
gboolean
li_runtime_load_directory (LiRuntime *rt, const gchar *dir, GError **error)
{
	gchar *uuid;
	_cleanup_object_unref_ GFile *ctlfile;
	_cleanup_free_ gchar *ctlpath;
	LiConfigData *cdata;
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	uuid = g_path_get_basename (dir);
	if (strlen (uuid) != 37) {
		g_warning ("Loading runtime with uuid '%s', which doesn't look valid.", uuid);
	}

	ctlpath = g_build_filename (dir, "control", NULL);
	ctlfile = g_file_new_for_path (ctlpath);
	if (!g_file_query_exists (ctlfile, NULL)) {
		g_set_error (error,
				G_FILE_ERROR,
				G_FILE_ERROR_NOENT,
				_("Runtime '%s' is not valid. Could not find control file."), uuid);
		g_free (uuid);
		return FALSE;

	}

	cdata = li_config_data_new ();
	li_config_data_load_file (cdata, ctlfile);
	li_runtime_fetch_values_from_cdata (rt, cdata);
	g_object_unref (cdata);

	g_free (priv->uuid);
	priv->uuid = uuid;

	priv->dir = g_strdup (dir);

	return TRUE;
}

/**
 * li_runtime_get_uuid:
 */
const gchar*
li_runtime_get_uuid (LiRuntime *rt)
{
	LiRuntimePrivate *priv = GET_PRIVATE (rt);
	return priv->uuid;
}

/**
 * li_utils_find_files_matching:
 */
static gboolean
li_runtime_link_software (const gchar* sw_dir, const gchar* frmw_destination, GError **error)
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

			li_runtime_link_software (path, dest_path, &tmp_error);
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
 * li_runtime_create_with_members:
 * @sw: A list of software as #LiPkgInfo
 *
 * Generate a new runtime environment consisting of the given
 * members.
 */
LiRuntime*
li_runtime_create_with_members (GPtrArray *members, GError **error)
{
	guint i;
	LiRuntime *rt;
	const gchar *uuid;
	gchar *rt_path;
	gboolean ret = TRUE;
	GError *tmp_error = NULL;

	rt = li_runtime_new ();

	uuid = li_runtime_get_uuid (rt);
	rt_path = g_build_filename (LI_SOFTWARE_ROOT, "tmp", uuid, "data", NULL);

	li_touch_dir (rt_path, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	for (i = 0; i < members->len; i++) {
		gchar *data_path;
		const gchar *pkid;
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (members, i));

		pkid = li_pkg_info_get_id (pki);
		if (pkid == NULL) {
			g_warning ("Found package without identifier!");
			continue;
		}

		data_path = g_build_filename (LI_SOFTWARE_ROOT, pkid, "data", NULL);
		li_runtime_link_software (data_path, rt_path, &tmp_error);
		g_free (data_path);

		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}
	}

out:
	g_free (rt_path);
	if (ret) {
		return rt;
	} else {
		g_object_unref (rt);
		return NULL;
	}
}

/**
 * li_runtime_class_init:
 **/
static void
li_runtime_class_init (LiRuntimeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_runtime_finalize;
}

/**
 * li_runtime_new:
 *
 * Creates a new #LiRuntime.
 *
 * Returns: (transfer full): a #LiRuntime
 *
 **/
LiRuntime *
li_runtime_new (void)
{
	LiRuntime *rt;
	rt = g_object_new (LI_TYPE_RUNTIME, NULL);
	return LI_RUNTIME (rt);
}
