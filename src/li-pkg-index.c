/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Matthias Klumpp <matthias@tenstral.net>
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
#include "li-utils-private.h"

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

		str = li_config_data_get_value (cdata, "Type");
		if (str != NULL) {
			LiPackageKind kind;
			kind = li_package_kind_from_string (str);
			li_pkg_info_set_kind (pki, kind);
			g_free (str);
		}

		str = li_config_data_get_value (cdata, "Name");
		li_pkg_info_set_appname (pki, str);
		g_free (str);

		str = li_config_data_get_value (cdata, "Version");
		li_pkg_info_set_version (pki, str);
		g_free (str);

		str = li_config_data_get_value (cdata, "Requires");
		li_pkg_info_set_dependencies (pki, str);
		g_free (str);

		str = li_config_data_get_value (cdata, "SHA256");
		li_pkg_info_set_checksum_sha256 (pki, str);
		g_free (str);

		str = li_config_data_get_value (cdata, "Location");
		li_pkg_info_set_repo_location (pki, str);
		g_free (str);

		/* mark package as available for installation */
		li_pkg_info_add_flag (pki, LI_PACKAGE_FLAG_AVAILABLE);
		g_ptr_array_add (priv->packages, pki);
	}
}

/**
 * li_pkg_index_write_cdata_values:
 **/
static void
li_pkg_index_write_cdata_values (LiPkgIndex *pkidx, LiConfigData *cdata)
{
	guint i;
	LiPkgIndexPrivate *priv = GET_PRIVATE (pkidx);

	/* write format info */
	li_config_data_set_value (cdata, "Format-Version", "1.0");

	for (i = 0; i < priv->packages->len; i++) {
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (priv->packages, i));

		li_config_data_new_block (cdata);
		li_config_data_set_value (cdata, "PkgName", li_pkg_info_get_name (pki));
		if (li_pkg_info_get_kind (pki) != LI_PACKAGE_KIND_COMMON)
			li_config_data_set_value (cdata, "Type", li_package_kind_to_string (li_pkg_info_get_kind (pki)));
		li_config_data_set_value (cdata, "Name", li_pkg_info_get_appname (pki));
		li_config_data_set_value (cdata, "Version", li_pkg_info_get_version (pki));
		li_config_data_set_value (cdata, "Requires", li_pkg_info_get_dependencies (pki));
		li_config_data_set_value (cdata, "SHA256", li_pkg_info_get_checksum_sha256 (pki));
		li_config_data_set_value (cdata, "Location", li_pkg_info_get_repo_location (pki));
	}
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
li_pkg_index_load_file (LiPkgIndex *pkidx, GFile *file, GError **error)
{
	GError *tmp_error = NULL;
	g_autoptr(LiConfigData) cdata = NULL;

	cdata = li_config_data_new ();

	li_config_data_load_file (cdata, file, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	li_pkg_index_fetch_values_from_cdata (pkidx, cdata);
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
 * li_pkg_index_get_data:
 */
gchar*
li_pkg_index_get_data (LiPkgIndex *pkidx)
{
	LiConfigData *cdata;
	gchar *res;

	cdata = li_config_data_new ();
	li_pkg_index_write_cdata_values (pkidx, cdata);
	res = li_config_data_get_data (cdata);
	g_object_unref (cdata);

	return res;
}

/**
 * li_pkg_index_get_packages:
 *
 * Returns: (transfer none) (element-type LiPkgInfo): Packages in the index
 */
GPtrArray*
li_pkg_index_get_packages (LiPkgIndex *pkidx)
{
	LiPkgIndexPrivate *priv = GET_PRIVATE (pkidx);
	return priv->packages;
}

/**
 * li_pkg_index_get_packages_count:
 *
 * Returns: Count of packages in the index
 */
guint
li_pkg_index_get_packages_count (LiPkgIndex *pkidx)
{
	LiPkgIndexPrivate *priv = GET_PRIVATE (pkidx);
	return priv->packages->len;
}

/**
 * li_pkg_index_add_package:
 */
void
li_pkg_index_add_package (LiPkgIndex *pkidx, LiPkgInfo *pki)
{
	LiPkgIndexPrivate *priv = GET_PRIVATE (pkidx);
	g_ptr_array_add (priv->packages, g_object_ref (pki));
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
