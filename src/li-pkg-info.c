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
#include "li-pkg-info.h"
#include "li-config-data.h"

typedef struct _LiPkgInfoPrivate	LiPkgInfoPrivate;
struct _LiPkgInfoPrivate
{
	gchar *format_version;
	gchar *id; /* auto-generated */
	gchar *version;
	gchar *name;
	gchar *framework_uuid;
	gchar *dependencies;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPkgInfo, li_pkg_info, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_pkg_info_get_instance_private (o))

/**
 * li_pkg_info_fetch_values_from_cdata:
 **/
static void
li_pkg_info_fetch_values_from_cdata (LiPkgInfo *pki, LiConfigData *cdata)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->id);
	priv->id = NULL;

	g_free (priv->name);
	priv->name = li_config_data_get_value (cdata, "Name");

	g_free (priv->version);
	priv->version = li_config_data_get_value (cdata, "Version");

	g_free (priv->dependencies);
	priv->dependencies = li_config_data_get_value (cdata, "Requires");

	g_free (priv->framework_uuid);
	priv->framework_uuid = li_config_data_get_value (cdata, "Framework-UUID");
}

/**
 * li_pkg_info_update_cdata_values:
 **/
static void
li_pkg_info_update_cdata_values (LiPkgInfo *pki, LiConfigData *cdata)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	if (priv->name != NULL)
		li_config_data_set_value (cdata, "Name", priv->name);

	if (priv->version != NULL)
		li_config_data_set_value (cdata, "Version", priv->version);

	if (priv->dependencies != NULL)
		li_config_data_set_value (cdata, "Requires", priv->dependencies);

	if (priv->framework_uuid != NULL)
		li_config_data_set_value (cdata, "Framework-UUID", priv->framework_uuid);
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
	g_free (priv->name);
	g_free (priv->version);
	g_free (priv->dependencies);
	g_free (priv->framework_uuid);

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
	ret = li_config_data_save_to_file (cdata, filename);
	g_object_unref (cdata);

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
 * li_pkg_info_get_depends_framework:
 */
const gchar*
li_pkg_info_get_framework_dependency (LiPkgInfo *pki)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);
	return priv->framework_uuid;
}

/**
 * li_pkg_info_set_depends_framework:
 */
void
li_pkg_info_set_framework_dependency (LiPkgInfo *pki, const gchar *uuid)
{
	LiPkgInfoPrivate *priv = GET_PRIVATE (pki);

	g_free (priv->framework_uuid);
	priv->framework_uuid = g_strdup (uuid);
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

	if ((priv->name == NULL) || (priv->version == NULL))
		return NULL;

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
