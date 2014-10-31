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
 * SECTION:li-ipk-control
 * @short_description: Control metadata for IPK packages
 */

#include "config.h"
#include "li-ipk-control.h"
#include "li-config-data.h"

typedef struct _LiIPKControlPrivate	LiIPKControlPrivate;
struct _LiIPKControlPrivate
{
	gchar *format_version;
	gchar *version;
	gchar *name;
	gchar *framework_uuid;
	gchar *dependencies;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiIPKControl, li_ipk_control, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_ipk_control_get_instance_private (o))

/**
 * li_ipk_control_fetch_values_from_cdata:
 **/
static void
li_ipk_control_fetch_values_from_cdata (LiIPKControl *ipkc, LiConfigData *cdata)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

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
 * li_ipk_control_update_cdata_values:
 **/
static void
li_ipk_control_update_cdata_values (LiIPKControl *ipkc, LiConfigData *cdata)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

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
 * li_ipk_control_finalize:
 **/
static void
li_ipk_control_finalize (GObject *object)
{
	LiIPKControl *ipkc = LI_IPK_CONTROL (object);
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

	g_free (priv->name);
	g_free (priv->version);
	g_free (priv->dependencies);
	g_free (priv->framework_uuid);

	G_OBJECT_CLASS (li_ipk_control_parent_class)->finalize (object);
}

/**
 * li_ipk_control_init:
 **/
static void
li_ipk_control_init (LiIPKControl *ipkc)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

	priv->version = NULL;
}

/**
 * li_ipk_control_load_data:
 */
void
li_ipk_control_load_data (LiIPKControl *ipkc, const gchar *data)
{
	LiConfigData *cdata;

	cdata = li_config_data_new ();
	li_config_data_load_data (cdata, data);
	li_ipk_control_fetch_values_from_cdata (ipkc, cdata);
	g_object_unref (cdata);
}

/**
 * li_ipk_control_load_file:
 */
void
li_ipk_control_load_file (LiIPKControl *ipkc, GFile *file)
{
	LiConfigData *cdata;

	cdata = li_config_data_new ();
	li_config_data_load_file (cdata, file);
	li_ipk_control_fetch_values_from_cdata (ipkc, cdata);
	g_object_unref (cdata);
}

/**
 * li_ipk_control_save_to_file:
 */
gboolean
li_ipk_control_save_to_file (LiIPKControl *ipkc, const gchar *filename)
{
	LiConfigData *cdata;
	gboolean ret;

	cdata = li_config_data_new ();
	li_ipk_control_update_cdata_values (ipkc, cdata);
	ret = li_config_data_save_to_file (cdata, filename);
	g_object_unref (cdata);

	return ret;
}

/**
 * li_ipk_control_get_pkg_version:
 *
 * Get the version for this package, if specified.
 */
const gchar*
li_ipk_control_get_pkg_version (LiIPKControl *ipkc)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	return priv->version;
}

/**
 * li_ipk_control_set_pkg_version:
 * @version: A version string
 *
 * Set the version of this package
 */
void
li_ipk_control_set_pkg_version (LiIPKControl *ipkc, const gchar *version)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	g_free (priv->version);
	priv->version = g_strdup (version);
}

/**
 * li_ipk_control_get_name:
 */
const gchar*
li_ipk_control_get_name (LiIPKControl *ipkc)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	return priv->name;
}

/**
 * li_ipk_control_set_name:
 */
void
li_ipk_control_set_name (LiIPKControl *ipkc, const gchar *name)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

	g_free (priv->name);
	priv->name = g_strdup (name);
}

/**
 * li_ipk_control_get_depends_framework:
 */
const gchar*
li_ipk_control_get_framework_dependency (LiIPKControl *ipkc)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	return priv->framework_uuid;
}

/**
 * li_ipk_control_set_depends_framework:
 */
void
li_ipk_control_set_framework_dependency (LiIPKControl *ipkc, const gchar *uuid)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

	g_free (priv->framework_uuid);
	priv->framework_uuid = g_strdup (uuid);
}

/**
 * li_ipk_control_get_dependencies:
 */
const gchar*
li_ipk_control_get_dependencies (LiIPKControl *ipkc)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	return priv->dependencies;
}

/**
 * li_ipk_control_set_dependencies:
 */
void
li_ipk_control_set_dependencies (LiIPKControl *ipkc, const gchar *deps_string)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

	g_free (priv->dependencies);
	priv->dependencies = g_strdup (deps_string);
}

/**
 * li_ipk_control_class_init:
 **/
static void
li_ipk_control_class_init (LiIPKControlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_ipk_control_finalize;
}

/**
 * li_ipk_control_new:
 *
 * Creates a new #LiIPKControl.
 *
 * Returns: (transfer full): a #LiIPKControl
 *
 **/
LiIPKControl *
li_ipk_control_new (void)
{
	LiIPKControl *ipkc;
	ipkc = g_object_new (LI_TYPE_IPK_CONTROL, NULL);
	return LI_IPK_CONTROL (ipkc);
}
