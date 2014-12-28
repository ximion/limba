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
 * SECTION:li-pkg-info
 * @short_description: Control metadata for IPK packages
 */

#include "config.h"
#include "li-utils.h"
#include "li-utils-private.h"
#include "li-pkg-info.h"
#include "li-config-data.h"

typedef struct _LiPkgInfoPrivate	LiPkgInfoPrivate;
struct _LiPkgInfoPrivate
{
	gchar *format_version;
	gchar *arch;
	gchar *id; /* auto-generated */
	gchar *version;
	gchar *name;
	gchar *app_name;
	gchar *runtime_uuid;
	gchar *dependencies;
	gchar *hash_sha256;

	LiPackageFlags flags;
	LiVersionFlags vrel;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPkgInfo, li_pkg_info, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_pkg_info_get_instance_private (o))

/**
 * li_str_to_bool:
 */
static inline gboolean
li_str_to_bool (const gchar *str)
{
	return g_strcmp0 (str, "true") == 0;
}

/**
 * li_pkg_info_fetch_values_from_cdata:
 **/
static void
li_pkg_info_fetch_values_from_cdata (LiPkgInfo *pki, LiConfigData *cdata)
{
	gchar *str;
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	li_config_data_reset (cdata);

	g_free (priv->format_version);
	priv->format_version = li_config_data_get_value (cdata, "Format-Version");

	/* jump to data block */
	li_config_data_next (cdata);

	g_free (priv->id);
	priv->id = NULL;

	g_free (priv->name);
	priv->name = li_config_data_get_value (cdata, "PkgName");

	g_free (priv->arch);
	priv->arch = li_config_data_get_value (cdata, "Architecture");

	g_free (priv->app_name);
	priv->app_name = li_config_data_get_value (cdata, "Name");

	g_free (priv->version);
	priv->version = li_config_data_get_value (cdata, "Version");

	g_free (priv->dependencies);
	priv->dependencies = li_config_data_get_value (cdata, "Requires");

	g_free (priv->runtime_uuid);
	priv->runtime_uuid = li_config_data_get_value (cdata, "Runtime-UUID");

	str = li_config_data_get_value (cdata, "Automatic");
	if (li_str_to_bool (str))
		li_pkg_info_add_flag (pki, LI_PACKAGE_FLAG_AUTOMATIC);
	g_free (str);

	/* a package with a %NULL architecture should never happen - assume the current one in that case */
	if (priv->arch == NULL)
		priv->arch = li_get_current_arch_h ();

	/* if we didn't get a format version, we assume 1.0 */
	if (priv->format_version == NULL)
		priv->format_version = g_strdup ("1.0");
}

/**
 * li_pkg_info_update_cdata_values:
 **/
static void
li_pkg_info_update_cdata_values (LiPkgInfo *pki, LiConfigData *cdata)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	li_config_data_reset (cdata);

	/* write format header */
	li_config_data_set_value (cdata, "Format-Version", priv->format_version);
	li_config_data_new_block (cdata);

	/* write actual data block */
	if (priv->name != NULL)
		li_config_data_set_value (cdata, "PkgName", priv->name);

	if (priv->arch != NULL)
		li_config_data_set_value (cdata, "Architecture", priv->arch);

	if (priv->app_name != NULL)
		li_config_data_set_value (cdata, "Name", priv->app_name);

	if (priv->version != NULL)
		li_config_data_set_value (cdata, "Version", priv->version);

	if (priv->dependencies != NULL)
		li_config_data_set_value (cdata, "Requires", priv->dependencies);

	if (priv->runtime_uuid != NULL)
		li_config_data_set_value (cdata, "Runtime-UUID", priv->runtime_uuid);

	if (priv->flags & LI_PACKAGE_FLAG_AUTOMATIC)
		li_config_data_set_value (cdata, "Automatic", "true");
}

/**
 * li_pkg_info_finalize:
 **/
static void
li_pkg_info_finalize (GObject *object)
{
	LiPkgInfo *pki = LI_PKG_INFO (object);
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->id);
	g_free (priv->arch);
	g_free (priv->name);
	g_free (priv->app_name);
	g_free (priv->version);
	g_free (priv->dependencies);
	g_free (priv->runtime_uuid);
	g_free (priv->format_version);

	G_OBJECT_CLASS (li_pkg_info_parent_class)->finalize (object);
}

/**
 * li_pkg_info_init:
 **/
static void
li_pkg_info_init (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	priv->id = NULL;
	priv->version = NULL;
	priv->flags = LI_PACKAGE_FLAG_NONE;
	priv->vrel = LI_VERSION_UNKNOWN;
	priv->arch = li_get_current_arch_h ();
	priv->format_version = g_strdup ("1.0");
}

/**
 * li_pkg_info_load_data:
 */
void
li_pkg_info_load_data (LiPkgInfo *pki, const gchar *data)
{
	LiConfigData *cdata;

	cdata = li_config_data_new ();
	li_config_data_load_data (cdata, data);
	li_pkg_info_fetch_values_from_cdata (pki, cdata);
	g_object_unref (cdata);
}

/**
 * li_pkg_info_load_file:
 */
void
li_pkg_info_load_file (LiPkgInfo *pki, GFile *file)
{
	LiConfigData *cdata;

	cdata = li_config_data_new ();
	li_config_data_load_file (cdata, file);
	li_pkg_info_fetch_values_from_cdata (pki, cdata);
	g_object_unref (cdata);
}

/**
 * li_pkg_info_save_to_file:
 */
gboolean
li_pkg_info_save_to_file (LiPkgInfo *pki, const gchar *filename)
{
	LiConfigData *cdata;
	gboolean ret;

	cdata = li_config_data_new ();
	li_pkg_info_update_cdata_values (pki, cdata);
	ret = li_config_data_save_to_file (cdata, filename, NULL);
	g_object_unref (cdata);

	return ret;
}

/**
 * li_pkg_info_save_changes:
 *
 * Save changes to the control file of the installed
 * package which matches the id of this #LiPkgInfo.
 * This does only override an existing file.
 */
gboolean
li_pkg_info_save_changes (LiPkgInfo *pki)
{
	gchar *fname;
	gboolean ret;
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	if (priv->id == NULL)
		return FALSE;

	fname = g_build_filename (LI_SOFTWARE_ROOT, priv->id, "control", NULL);
	if (!g_file_test (fname, G_FILE_TEST_EXISTS)) {
		g_free (fname);
		return FALSE;
	}

	ret = li_pkg_info_save_to_file (pki, fname);
	g_free (fname);

	return ret;
}

/**
 * li_pkg_info_get_version:
 *
 * Get the version for this package, if specified.
 */
const gchar*
li_pkg_info_get_version (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->version;
}

/**
 * li_pkg_info_set_version:
 * @version: A version string
 *
 * Set the version of this package
 */
void
li_pkg_info_set_version (LiPkgInfo *pki, const gchar *version)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->version);
	priv->version = g_strdup (version);

	/* we need to re-generate the id */
	g_free (priv->id);
	priv->id = NULL;
}

/**
 * li_pkg_info_get_name:
 */
const gchar*
li_pkg_info_get_name (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->name;
}

/**
 * li_pkg_info_set_name:
 */
void
li_pkg_info_set_name (LiPkgInfo *pki, const gchar *name)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->name);
	priv->name = g_strdup (name);

	/* we need to re-generate the id */
	g_free (priv->id);
	priv->id = NULL;
}

/**
 * li_pkg_info_get_appname:
 *
 * Get a human-friendly full name of this software.
 */
const gchar*
li_pkg_info_get_appname (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	if (priv->app_name == NULL)
		return priv->name;
	return priv->app_name;
}

/**
 * li_pkg_info_set_appname:
 */
void
li_pkg_info_set_appname (LiPkgInfo *pki, const gchar *app_name)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	g_free (priv->app_name);
	priv->app_name = g_strdup (app_name);
}

/**
 * li_pkg_info_get_runtime_dependency:
 */
const gchar*
li_pkg_info_get_runtime_dependency (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->runtime_uuid;
}

/**
 * li_pkg_info_set_runtime_dependency:
 */
void
li_pkg_info_set_runtime_dependency (LiPkgInfo *pki, const gchar *uuid)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->runtime_uuid);
	priv->runtime_uuid = g_strdup (uuid);
}

/**
 * li_pkg_info_get_dependencies:
 */
const gchar*
li_pkg_info_get_dependencies (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->dependencies;
}

/**
 * li_pkg_info_get_id:
 */
const gchar*
li_pkg_info_get_id (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	if ((priv->name == NULL) || (priv->version == NULL)) {
		/* an empty package-id is a serious issue and usually a bug */
		g_warning ("Queried empty package-id.");
		return NULL;
	}

	/* re-generate id if necessary */
	if (priv->id == NULL)
		priv->id = g_strdup_printf ("%s-%s", priv->name, priv->version);

	return priv->id;
}

/**
 * li_pkg_info_set_dependencies:
 */
void
li_pkg_info_set_dependencies (LiPkgInfo *pki, const gchar *deps_string)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->dependencies);
	priv->dependencies = g_strdup (deps_string);
}

/**
 * li_pkg_info_get_checksum_sha256:
 *
 * The SHA256 checksum of the package referenced by this package-info.
 * This is usually used in package-indices.
 */
const gchar*
li_pkg_info_get_checksum_sha256 (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->hash_sha256;
}

/**
 * li_pkg_info_set_checksum_sha256:
 */
void
li_pkg_info_set_checksum_sha256 (LiPkgInfo *pki, const gchar *hash)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	g_free (priv->hash_sha256);
	priv->hash_sha256 = g_strdup (hash);
}

/**
 * li_pkg_info_set_flags:
 */
void
li_pkg_info_set_flags (LiPkgInfo *pki, LiPackageFlags flags)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	priv->flags = flags;
}

/**
 * li_pkg_info_add_flag:
 */
void
li_pkg_info_add_flag (LiPkgInfo *pki, LiPackageFlags flag)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	priv->flags |= flag;
}

/**
 * li_pkg_info_get_flags:
 */
LiPackageFlags
li_pkg_info_get_flags (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->flags;
}

/**
 * li_pkg_info_set_version_relation:
 */
void
li_pkg_info_set_version_relation (LiPkgInfo *pki, LiVersionFlags vrel)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	priv->vrel = vrel;
}

/**
 * li_pkg_info_get_version_relation:
 */
LiVersionFlags
li_pkg_info_get_version_relation (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->vrel;
}

/**
 * li_pkg_info_get_architecture:
 *
 * Get the architecture this package was built for.
 */
const gchar*
li_pkg_info_get_architecture (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->arch;
}

/**
 * li_pkg_info_set_architecture:
 */
void
li_pkg_info_set_architecture (LiPkgInfo *pki, const gchar *arch)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	g_free (priv->arch);
	priv->arch = g_strdup (arch);
}

/**
 * li_pkg_info_matches_current_arch:
 *
 * Returns: %TRUE if package is suitable for the current system architecture
 */
gboolean
li_pkg_info_matches_current_arch (LiPkgInfo *pki)
{
	_cleanup_free_ gchar *c_arch = NULL;
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	c_arch = li_get_current_arch_h ();
	return (g_strcmp0 (priv->arch, "all") == 0) || (g_strcmp0 (priv->arch, c_arch) == 0);
}

/**
 * li_pkg_info_class_init:
 **/
static void
li_pkg_info_class_init (LiPkgInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_pkg_info_finalize;
}

/**
 * li_pkg_info_new:
 *
 * Creates a new #LiPkgInfo.
 *
 * Returns: (transfer full): a #LiPkgInfo
 *
 **/
LiPkgInfo *
li_pkg_info_new (void)
{
	LiPkgInfo *pki;
	pki = g_object_new (LI_TYPE_PKG_INFO, NULL);
	return LI_PKG_INFO (pki);
}
