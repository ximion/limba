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
	GHashTable *members;
	GHashTable *requirements;
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

	g_hash_table_remove_all (priv->members);
	g_hash_table_remove_all (priv->requirements);

	tmp = li_config_data_get_value (cdata, "Members");
	if (tmp != NULL) {
		guint i;
		gchar **strv;
		strv = g_strsplit (tmp, ",", -1);

		for (i = 0; strv[i] != NULL; i++) {
			g_strstrip (strv[i]);
			g_hash_table_add (priv->members, g_strdup (strv[i]));
		}
	}
	g_free (tmp);

	tmp = li_config_data_get_value (cdata, "Requirements");
	if (tmp != NULL) {
		guint i;
		gchar **strv;
		strv = g_strsplit (tmp, ",", -1);

		for (i = 0; strv[i] != NULL; i++) {
			g_strstrip (strv[i]);
			g_hash_table_add (priv->requirements, g_strdup (strv[i]));
		}
	}
	g_free (tmp);
}

/**
 * li_runtime_update_cdata_values:
 **/
static void
li_runtime_update_cdata_values (LiRuntime *rt, LiConfigData *cdata)
{
	guint i;
	GString *str;
	gchar **strv;
	guint len;
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	str = g_string_new ("");
	strv = (gchar**) g_hash_table_get_keys_as_array (priv->members, &len);
	for (i=0; i < len; i++) {
		g_string_append_printf (str, "%s, ", strv[i]);
	}
	g_free (strv);

	if (str->len > 0) {
		g_string_truncate (str, str->len - 2);
		li_config_data_set_value (cdata, "Members", str->str);
	}
	g_string_free (str, TRUE);

	str = g_string_new ("");
	strv = (gchar**) g_hash_table_get_keys_as_array (priv->requirements, &len);
	for (i=0; i < len; i++) {
		g_string_append_printf (str, "%s, ", strv[i]);
	}
	g_free (strv);

	if (str->len > 0) {
		g_string_truncate (str, str->len - 2);
		li_config_data_set_value (cdata, "Requirements", str->str);
	}
	g_string_free (str, TRUE);
}

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
	g_hash_table_unref (priv->requirements);
	g_hash_table_unref (priv->members);

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
	priv->requirements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	priv->members = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

/**
 * li_runtime_load_directory:
 */
gboolean
li_runtime_load_directory (LiRuntime *rt, const gchar *dir, GError **error)
{
	gchar *uuid;
	_cleanup_object_unref_ GFile *ctlfile;
	_cleanup_free_ gchar *ctlpath = NULL;
	_cleanup_object_unref_ LiConfigData *cdata = NULL;
	GError *tmp_error = NULL;
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	uuid = g_path_get_basename (dir);
	if (strlen (uuid) != 36) {
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
	li_config_data_load_file (cdata, ctlfile, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_free (uuid);
		return FALSE;
	}

	li_runtime_fetch_values_from_cdata (rt, cdata);

	g_free (priv->uuid);
	priv->uuid = uuid;

	priv->dir = g_strdup (dir);

	return TRUE;
}

/**
 * li_runtime_load_by_uuid:
 */
gboolean
li_runtime_load_by_uuid (LiRuntime *rt, const gchar *uuid, GError **error)
{
	_cleanup_free_ gchar *runtime_dir = NULL;
	gboolean ret;
	GError *tmp_error = NULL;

	if (strlen (uuid) != 36) {
		g_warning ("Loading runtime with uuid '%s', which doesn't look valid.", uuid);
	}

	runtime_dir = g_build_filename (LI_SOFTWARE_ROOT, "runtimes", uuid, NULL);

	ret = li_runtime_load_directory (rt, runtime_dir, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
	}

	return ret;
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
 * li_runtime_get_members:
 *
 * Returns: (transfer none) (element-type LiPkgInfo, LiPkgInfo): Hash set of packages which are members of this runtime
 */
GHashTable*
li_runtime_get_members (LiRuntime *rt)
{
	LiRuntimePrivate *priv = GET_PRIVATE (rt);
	return priv->members;
}

/**
 * li_runtime_get_requirements:
 *
 * Returns: (transfer none) (element-type LiPkgInfo, LiPkgInfo): Hash set of package requirements which make up this runtime
 */
GHashTable*
li_runtime_get_requirements (LiRuntime *rt)
{
	LiRuntimePrivate *priv = GET_PRIVATE (rt);
	return priv->requirements;
}

/**
 * li_runtime_add_package:
 */
void
li_runtime_add_package (LiRuntime *rt, LiPkgInfo *pki)
{
	LiRuntimePrivate *priv = GET_PRIVATE (rt);
	g_hash_table_add (priv->members,
						g_strdup (li_pkg_info_get_id (pki)));
	g_hash_table_add (priv->requirements,
						li_pkg_info_get_name_relation_string (pki));
}

/**
 * li_runtime_remove_package:
 */
void
li_runtime_remove_package (LiRuntime *rt, LiPkgInfo *pki)
{
	LiRuntimePrivate *priv = GET_PRIVATE (rt);
	g_hash_table_remove (priv->members,
						g_strdup (li_pkg_info_get_id (pki)));
	g_hash_table_remove (priv->requirements,
						li_pkg_info_get_name_relation_string (pki));
}

/**
 * li_runtime_save:
 *
 * Save the runtime metadata.
 */
gboolean
li_runtime_save (LiRuntime *rt, GError **error)
{
	gboolean ret;
	LiConfigData *cdata;
	_cleanup_free_ gchar *control_fname = NULL;
	_cleanup_free_ gchar *rt_path = NULL;
	GError *tmp_error = NULL;
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	rt_path = g_build_filename (LI_SOFTWARE_ROOT, "runtimes", priv->uuid, NULL);
	control_fname = g_build_filename (rt_path, "control", NULL);

	if (!g_file_test (rt_path, G_FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents (rt_path, 0755) != 0) {
			g_set_error (error,
				G_FILE_ERROR,
				G_FILE_ERROR_FAILED,
				_("Could not create directory structure for runtime. %s"), g_strerror (errno));
			return FALSE;
		}
	}

	cdata = li_config_data_new ();
	li_runtime_update_cdata_values (rt, cdata);
	ret = li_config_data_save_to_file (cdata, control_fname, &tmp_error);
	g_object_unref (cdata);

	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
	}

	return ret;
}

/**
 * li_runtime_create_with_members:
 * @members: (element-type LiPkgInfo): A list of software as #LiPkgInfo
 *
 * Generate a new runtime environment consisting of the given
 * members.
 *
 * Returns: (transfer full): A new #LiRuntime instance
 */
LiRuntime*
li_runtime_create_with_members (GPtrArray *members, GError **error)
{
	guint i;
	LiRuntime *rt;
	gboolean ret = TRUE;
	_cleanup_hashtable_unref_ GHashTable *dedup;
	GError *tmp_error = NULL;

	dedup = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	rt = li_runtime_new ();
	for (i = 0; i < members->len; i++) {
		const gchar *pkid;
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (members, i));

		pkid = li_pkg_info_get_id (pki);
		if (pkid == NULL) {
			g_warning ("Found package without identifier!");
			continue;
		}

		/* we can't add system dependencies to a runtime, so filter them out */
		if (g_str_has_prefix (li_pkg_info_get_name (pki), "foundation:")) {
			continue;
		}

		/* did we add this already? */
		if (!g_hash_table_add (dedup, g_strdup (pkid)))
			continue;

		/* register the added member with the runtime */
		li_runtime_add_package (rt, pki);
	}

	/* store metadata on disk */
	ret = li_runtime_save (rt, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
	}

	/* finish */
	if (ret) {
		return rt;
	} else {
		g_object_unref (rt);
		return NULL;
	}
}

/**
 * li_runtime_remove:
 *
 * Uninstall this runtime.
 */
gboolean
li_runtime_remove (LiRuntime *rt)
{
	gboolean ret;
	LiRuntimePrivate *priv = GET_PRIVATE (rt);

	if (priv->dir == NULL)
		return FALSE;

	ret = li_delete_dir_recursive (priv->dir);
	if (ret) {
		g_free (priv->dir);
		priv->dir = NULL;
	}

	return ret;
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
