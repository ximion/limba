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
 * SECTION:li-pkg-index
 * @short_description: A list of packages
 */

#include "config.h"
#include "li-pkg-index.h"
#include "li-config-data.h"

typedef struct _LiPkgIndexPrivate	LiPkgIndexPrivate;
struct _LiPkgIndexPrivate
{
	gchar *format_version;
	GPtrArray *packages;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPkgIndex, li_pkg_index, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_pkg_index_get_instance_private (o))

/**
 * li_pkg_index_fetch_values_from_cdata:
 **/
static void
li_pkg_index_fetch_values_from_cdata (LiPkgIndex *pkidx, LiConfigData *cdata)
{
	LiPkgIndexPrivate *priv = GET_PRIVATE (pkidx);

	li_config_data_open_block (cdata, "Format-Version", NULL, TRUE);
	g_free (priv->format_version);
	priv->format_version = li_config_data_get_value (cdata, "Format-Version");

	while (li_config_data_next (cdata)) {
		LiPkgInfo *pki;
		gchar *str;
		pki = li_pkg_info_new ();

		str = li_config_data_get_value (cdata, "PkgName");
		li_pkg_info_set_name (pki, str);
		g_free (str);

		str = li_config_data_get_value (cdata, "Name");
		li_pkg_info_set_appname (pki, str);
		g_free (str);

		str = li_config_data_get_value (cdata, "Version");
		li_pkg_info_set_version (pki, str);
		g_free (str);

		str = li_config_data_get_value (cdata, "SHA256");
		li_pkg_info_set_checksum_sha256 (pki, str);
		g_free (str);

		g_ptr_array_add (priv->packages, pki);
	}
}

/**
 * li_pkg_index_write_cdata_values:
 **/
static void
li_pkg_index_write_cdata_values (LiPkgIndex *pkidx, LiConfigData *cdata)
{
	// TODO
}

/**
 * li_pkg_index_finalize:
 **/
static void
li_pkg_index_finalize (GObject *object)
{
	LiPkgIndex *pkidx = LI_PKG_INDEX (object);
	LiPkgIndexPrivate *priv = GET_PRIVATE (pkidx);

	g_ptr_array_unref (priv->packages);

	G_OBJECT_CLASS (li_pkg_index_parent_class)->finalize (object);
}

/**
 * li_pkg_index_init:
 **/
static void
li_pkg_index_init (LiPkgIndex *pkidx)
{
	LiPkgIndexPrivate *priv = GET_PRIVATE (pkidx);

	priv->packages = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * li_pkg_index_load_data:
 */
void
li_pkg_index_load_data (LiPkgIndex *pkidx, const gchar *data)
{
	LiConfigData *cdata;

	cdata = li_config_data_new ();
	li_config_data_load_data (cdata, data);
	li_pkg_index_fetch_values_from_cdata (pkidx, cdata);
	g_object_unref (cdata);
}

/**
 * li_pkg_index_load_file:
 */
void
li_pkg_index_load_file (LiPkgIndex *pkidx, GFile *file)
{
	LiConfigData *cdata;

	cdata = li_config_data_new ();
	li_config_data_load_file (cdata, file);
	li_pkg_index_fetch_values_from_cdata (pkidx, cdata);
	g_object_unref (cdata);
}

/**
 * li_pkg_index_save_to_file:
 */
gboolean
li_pkg_index_save_to_file (LiPkgIndex *pkidx, const gchar *filename)
{
	LiConfigData *cdata;
	gboolean ret;

	cdata = li_config_data_new ();
	li_pkg_index_write_cdata_values (pkidx, cdata);
	ret = li_config_data_save_to_file (cdata, filename, NULL);
	g_object_unref (cdata);

	return ret;
}

/**
 * li_pkg_index_get_packages:
 */
GPtrArray*
li_pkg_index_get_packages (LiPkgIndex *pkidx)
{
	LiPkgIndexPrivate *priv = GET_PRIVATE (pkidx);
	return priv->packages;
}

/**
 * li_pkg_index_class_init:
 **/
static void
li_pkg_index_class_init (LiPkgIndexClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_pkg_index_finalize;
}

/**
 * li_pkg_index_new:
 *
 * Creates a new #LiPkgIndex.
 *
 * Returns: (transfer full): a #LiPkgIndex
 *
 **/
LiPkgIndex *
li_pkg_index_new (void)
{
	LiPkgIndex *pkidx;
	pkidx = g_object_new (LI_TYPE_PKG_INDEX, NULL);
	return LI_PKG_INDEX (pkidx);
}
