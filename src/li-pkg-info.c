/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Matthias Klumpp <matthias@tenstral.net>
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
	gchar *hash_sha256;
	gchar *repo_location;
	gchar *cpt_kind;
	gchar *abi_break_versions;

	gchar *dependencies;
	gchar *sdk_dependencies;
	gchar *build_dependencies;

	LiPackageKind kind;
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
	str = li_config_data_get_value (cdata, "Type");
	if (str != NULL) {
		priv->kind = li_package_kind_from_string (str);
		g_free (str);
	}

	/* jump to data block */
	li_config_data_next (cdata);

	g_free (priv->id);
	priv->id = NULL;

	str = li_config_data_get_value (cdata, "PkgName");
	if (str != NULL) {
		g_free (priv->name);
		priv->name = str;
	}

	str = li_config_data_get_value (cdata, "Name");
	if (str != NULL) {
		g_free (priv->app_name);
		priv->app_name = str;
	}

	str = li_config_data_get_value (cdata, "Version");
	if (str != NULL) {
		g_free (priv->version);
		priv->version = str;
	}

	str = li_config_data_get_value (cdata, "ABI-Break-Versions");
	if (str != NULL) {
		g_free (priv->abi_break_versions);
		priv->abi_break_versions = str;
	}

	g_free (priv->arch);
	priv->arch = li_config_data_get_value (cdata, "Architecture");

	g_free (priv->dependencies);
	priv->dependencies = li_config_data_get_value (cdata, "Requires");

	g_free (priv->sdk_dependencies);
	priv->sdk_dependencies = li_config_data_get_value (cdata, "SDK-Requires");

	g_free (priv->build_dependencies);
	priv->build_dependencies = li_config_data_get_value (cdata, "Build-Requires");

	g_free (priv->runtime_uuid);
	priv->runtime_uuid = li_config_data_get_value (cdata, "Runtime-UUID");

	g_free (priv->cpt_kind);
	priv->cpt_kind = li_config_data_get_value (cdata, "Component-Type");

	str = li_config_data_get_value (cdata, "Automatic");
	if (li_str_to_bool (str))
		li_pkg_info_add_flag (pki, LI_PACKAGE_FLAG_AUTOMATIC);
	g_free (str);

	str = li_config_data_get_value (cdata, "Faded");
	if (li_str_to_bool (str))
		li_pkg_info_add_flag (pki, LI_PACKAGE_FLAG_FADED);
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
	if (priv->kind != LI_PACKAGE_KIND_COMMON)
		li_config_data_set_value (cdata, "Type", li_package_kind_to_string (priv->kind));
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

	if (priv->cpt_kind != NULL)
		li_config_data_set_value (cdata, "Component-Type", priv->cpt_kind);

	if (priv->abi_break_versions != NULL)
		li_config_data_set_value (cdata, "ABI-Break-Versions", priv->abi_break_versions);

	if (priv->dependencies != NULL)
		li_config_data_set_value (cdata, "Requires", priv->dependencies);

	if (priv->sdk_dependencies != NULL)
		li_config_data_set_value (cdata, "SDK-Requires", priv->sdk_dependencies);

	if (priv->build_dependencies != NULL)
		li_config_data_set_value (cdata, "Build-Requires", priv->build_dependencies);

	if (priv->runtime_uuid != NULL)
		li_config_data_set_value (cdata, "Runtime-UUID", priv->runtime_uuid);

	if (priv->flags & LI_PACKAGE_FLAG_AUTOMATIC)
		li_config_data_set_value (cdata, "Automatic", "true");

	if (priv->flags & LI_PACKAGE_FLAG_FADED)
		li_config_data_set_value (cdata, "Faded", "true");
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
	g_free (priv->repo_location);
	g_free (priv->hash_sha256);
	g_free (priv->abi_break_versions);

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

	/* for compatibility reasons, we assume the package to be normal by default */
	priv->kind = LI_PACKAGE_KIND_COMMON;
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
li_pkg_info_load_file (LiPkgInfo *pki, GFile *file, GError **error)
{
	GError *tmp_error = NULL;
	g_autoptr(LiConfigData) cdata = NULL;

	cdata = li_config_data_new ();

	li_config_data_load_file (cdata, file, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	li_pkg_info_fetch_values_from_cdata (pki, cdata);
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
 * li_pkg_info_get_id:
 */
const gchar*
li_pkg_info_get_id (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	if (priv->id == NULL) {
		if ((priv->name == NULL) || (priv->version == NULL)) {
			/* an empty package-id is a serious issue and usually a bug */
			g_warning ("Queried empty package-id.");
			return NULL;
		}

		/* re-generate id if necessary */
		priv->id = g_strdup_printf ("%s/%s", priv->name, priv->version);
	}

	return priv->id;
}

/**
 * li_pkg_info_set_id:
 */
void
li_pkg_info_set_id (LiPkgInfo *pki, const gchar *id)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	priv->id = g_strdup (id);
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
 * li_pkg_info_set_sdk_dependencies:
 *
 * Set dependencies used by the development version of this package.
 * This is only useful for IPK source packages.
 */
void
li_pkg_info_set_sdk_dependencies (LiPkgInfo *pki, const gchar *deps_string)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->sdk_dependencies);
	priv->sdk_dependencies = g_strdup (deps_string);
}

/**
 * li_pkg_info_get_sdk_dependencies:
 *
 * Dependencies used by the development version of this package.
 * This is only useful for IPK source packages.
 */
const gchar*
li_pkg_info_get_sdk_dependencies (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->sdk_dependencies;
}

/**
 * li_pkg_info_set_build_dependencies:
 * @pki: An instance of #LiPkgInfo
 *
 * Set dependencies needed to build this package.
 */
void
li_pkg_info_set_build_dependencies (LiPkgInfo *pki, const gchar *deps_string)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->build_dependencies);
	priv->build_dependencies = g_strdup (deps_string);
}

/**
 * li_pkg_info_get_build_dependencies:
 * @pki: An instance of #LiPkgInfo
 *
 * Dependencies needed to build this package.
 */
const gchar*
li_pkg_info_get_build_dependencies (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->build_dependencies;
}

/**
 * li_pkg_info_get_checksum_sha256:
 * @pki: An instance of #LiPkgInfo
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
 * @pki: An instance of #LiPkgInfo
 */
void
li_pkg_info_set_checksum_sha256 (LiPkgInfo *pki, const gchar *hash)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	g_free (priv->hash_sha256);
	priv->hash_sha256 = g_strdup (hash);
}

/**
 * li_pkg_info_get_kind:
 * @pki: An instance of #LiPkgInfo
 */
LiPackageKind
li_pkg_info_get_kind (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->kind;
}

/**
 * li_pkg_info_set_kind:
 * @pki: An instance of #LiPkgInfo
 */
void
li_pkg_info_set_kind (LiPkgInfo *pki, LiPackageKind kind)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	priv->kind = kind;
}

/**
 * li_pkg_info_get_component_kind:
 * @pki: An instance of #LiPkgInfo
 *
 * The AppStream component kind of the software component this package contains.
 * You can use as_component_kind_from_string() to convert it into its enum representation.
 */
const gchar*
li_pkg_info_get_component_kind (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->cpt_kind;
}

/**
 * li_pkg_info_set_component_kind:
 * @pki: An instance of #LiPkgInfo
 */
void
li_pkg_info_set_component_kind (LiPkgInfo *pki, const gchar *kind)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	g_free (priv->cpt_kind);
	priv->cpt_kind = g_strdup (kind);
}

/**
 * li_pkg_info_set_flags:
 * @pki: An instance of #LiPkgInfo
 */
void
li_pkg_info_set_flags (LiPkgInfo *pki, LiPackageFlags flags)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	priv->flags = flags;
}

/**
 * li_pkg_info_get_flags:
 * @pki: An instance of #LiPkgInfo
 */
LiPackageFlags
li_pkg_info_get_flags (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->flags;
}

/**
 * li_pkg_info_add_flag:
 * @pki: An instance of #LiPkgInfo
 */
void
li_pkg_info_add_flag (LiPkgInfo *pki, LiPackageFlags flag)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	if ((flag == LI_PACKAGE_FLAG_INSTALLED) && (li_pkg_info_has_flag (pki, LI_PACKAGE_FLAG_AVAILABLE)))
		g_warning ("Trying to set bad package flags: INSTALLED add to package which already has AVAILABLE flag.");
	if ((flag == LI_PACKAGE_FLAG_AVAILABLE) && (li_pkg_info_has_flag (pki, LI_PACKAGE_FLAG_INSTALLED)))
		g_warning ("Trying to set bad package flags: AVAILABLE add to package which already has INSTALLED flag.");

	priv->flags |= flag;
}

/**
 * li_pkg_info_has_flag:
 * @pki: An instance of #LiPkgInfo
 * @flag: #LiPackageFlag to check for.
 *
 * Returns: %TRUE if the flag is assigned.
 */
gboolean
li_pkg_info_has_flag (LiPkgInfo *pki, LiPackageFlags flag)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->flags & flag;
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
 * li_pkg_info_get_name_relation_string:
 *
 * Get the package name and relation as string, e.g. "foobar >= 2.1"
 *
 * Returns: The name/relation string, free with g_free()
 */
gchar*
li_pkg_info_get_name_relation_string (LiPkgInfo *pki)
{
	gchar *relation = NULL;
	gchar *tmp;
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	if (priv->vrel & LI_VERSION_LOWER)
		relation = g_strnfill (2, '<');
	if (priv->vrel & LI_VERSION_HIGHER)
		relation = g_strnfill (2, '>');
	if (relation == NULL)
		relation = g_strnfill (2, '=');
	if (priv->vrel & LI_VERSION_EQUAL)
		relation[1] = '=';

	tmp = g_strdup_printf ("%s (%s %s)",
				li_pkg_info_get_name (pki),
				relation,
				li_pkg_info_get_version (pki));
	g_free (relation);

	return tmp;
}

/**
 * li_pkg_info_satisfies_requirement:
 *
 * Check if the current package @pki matches the requirements defined
 * by #LiPkgInfo @req.
 *
 * Returns: %TRUE if package satisfies requirements.
 */
gboolean
li_pkg_info_satisfies_requirement (LiPkgInfo *pki, LiPkgInfo *req)
{
	const gchar *req_name;
	const gchar *req_version;
	LiVersionFlags req_vrel;
	gint cmp;
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	req_name = li_pkg_info_get_name (req);
	req_version = li_pkg_info_get_version (req);
	req_vrel = li_pkg_info_get_version_relation (req);

	/* check if names match */
	if (g_strcmp0 (priv->name, req_name) != 0)
		return FALSE;

	if (req_version == NULL) {
		/* any version satisfies this dependency - so we are happy already */
		return TRUE;
	}

	/* now verify that its version is sufficient */
	cmp = li_compare_versions (priv->version, req_version);
	if (((cmp == 1) && (req_vrel & LI_VERSION_HIGHER)) ||
		((cmp == 0) && (req_vrel & LI_VERSION_EQUAL)) ||
		((cmp == -1) && (req_vrel & LI_VERSION_LOWER))) {
		/* we are good, this package satisfies the requirements */
		return TRUE;
	} else {
		return FALSE;
	}
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
	g_autofree gchar *c_arch = NULL;
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	c_arch = li_get_current_arch_h ();
	return (g_strcmp0 (priv->arch, "all") == 0) || (g_strcmp0 (priv->arch, c_arch) == 0);
}

/**
 * li_pkg_info_get_repo_location:
 *
 * Get the location of this package in the pool of a repository.
 */
const gchar*
li_pkg_info_get_repo_location (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->repo_location;
}

/**
 * li_pkg_info_set_repo_location:
 */
void
li_pkg_info_set_repo_location (LiPkgInfo *pki, const gchar *location)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	g_free (priv->repo_location);
	priv->repo_location = g_strdup (location);
}

/**
 * li_pkg_info_get_abi_break_versions:
 */
const gchar*
li_pkg_info_get_abi_break_versions (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->abi_break_versions;
}

/**
 * li_pkg_info_set_abi_break_versions:
 */
void
li_pkg_info_set_abi_break_versions (LiPkgInfo *pki, const gchar *versions)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	g_free (priv->abi_break_versions);
	priv->abi_break_versions = g_strdup (versions);
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

/**
 * li_package_kind_to_string:
 * @kind: the %LiPackageKind.
 *
 * Converts the enumerated value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar*
li_package_kind_to_string (LiPackageKind kind)
{
	if (kind == LI_PACKAGE_KIND_COMMON)
		return "common";
	if (kind == LI_PACKAGE_KIND_DEVEL)
		return "devel";
	return "unknown";
}

/**
 * li_package_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a %LiPackageKind or %LI_PACKAGE_KIND_UNKNOWN for unknown
 **/
LiPackageKind
li_package_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "common") == 0)
		return LI_PACKAGE_KIND_COMMON;
	if (g_strcmp0 (kind_str, "") == 0)
		return LI_PACKAGE_KIND_COMMON;
	if (g_strcmp0 (kind_str, "devel") == 0)
		return LI_PACKAGE_KIND_DEVEL;
	return LI_PACKAGE_KIND_UNKNOWN;
}
