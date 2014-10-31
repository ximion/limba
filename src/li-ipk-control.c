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
	LiConfigData *cdata;

	/* cached data */
	gchar *pkg_version;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiIPKControl, li_ipk_control, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_ipk_control_get_instance_private (o))

/**
 * li_ipk_control_init:
 **/
static void
li_ipk_control_delete_cached_data (LiIPKControl *ipkc)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	if (priv->pkg_version != NULL)
		g_free (priv->pkg_version);
	priv->pkg_version = NULL;
}

/**
 * li_ipk_control_finalize:
 **/
static void
li_ipk_control_finalize (GObject *object)
{
	LiIPKControl *ipkc = LI_IPK_CONTROL (object);
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

	g_object_unref (priv->cdata);
	li_ipk_control_delete_cached_data (ipkc);

	G_OBJECT_CLASS (li_ipk_control_parent_class)->finalize (object);
}

/**
 * li_ipk_control_init:
 **/
static void
li_ipk_control_init (LiIPKControl *ipkc)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	priv->cdata = li_config_data_new ();
}

/**
 * li_ipk_control_load_data:
 */
void
li_ipk_control_load_data (LiIPKControl *ipkc, const gchar *data)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	li_config_data_load_data (priv->cdata, data);

	li_ipk_control_delete_cached_data (ipkc);
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
	if (priv->pkg_version != NULL)
		return priv->pkg_version;
	priv->pkg_version = li_config_data_get_value (priv->cdata, "PkgVersion");
	return priv->pkg_version;
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
	li_config_data_set_value (priv->cdata, "PkgVersion", version);

	/* remove cached value */
	if (priv->pkg_version != NULL)
		g_free (priv->pkg_version);
	priv->pkg_version = NULL;
}

/**
 * li_ipk_control_save_to_file:
 */
gboolean
li_ipk_control_save_to_file (LiIPKControl *ipkc, const gchar *filename)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	return li_config_data_save_to_file (priv->cdata, filename);
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
