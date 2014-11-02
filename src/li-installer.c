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
 * SECTION:li-installer
 * @short_description: High level installation of IPK packages
 */

#include "config.h"
#include "li-installer.h"

#include <glib/gi18n-lib.h>

#include "li-pkg-info.h"
#include "li-ipk-package.h"
#include "li-manager.h"
#include "li-polylinker.h"

typedef struct _LiInstallerPrivate	LiInstallerPrivate;
struct _LiInstallerPrivate
{
	LiPolylinker *plink;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiInstaller, li_installer, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_installer_get_instance_private (o))

/**
 * li_installer_finalize:
 **/
static void
li_installer_finalize (GObject *object)
{
	LiInstaller *inst = LI_INSTALLER (object);
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	g_object_unref (priv->plink);

	G_OBJECT_CLASS (li_installer_parent_class)->finalize (object);
}

/**
 * li_installer_init:
 **/
static void
li_installer_init (LiInstaller *inst)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	priv->plink = li_polylinker_new ();
}

/**
 * li_installer_parse_dependency_string:
 */
static GPtrArray*
li_installer_parse_dependency_string (const gchar *depstr)
{
	guint i;
	gchar **slices;
	GPtrArray *array;

	if (depstr == NULL)
		return NULL;

	array = g_ptr_array_new_with_free_func (g_object_unref);
	slices = g_strsplit (depstr, ",", -1);

	for (i = 0; slices[i] != NULL; i++) {
		gchar *dep_raw;
		//gchar *dep_version;
		LiPkgInfo *ipkc;

		g_strstrip (slices[i]);
		dep_raw = slices[i];

		ipkc = li_pkg_info_new ();
		if (g_strrstr (dep_raw, "(") != NULL) {
			gchar **strv;
			//gchar *ver_tmp;

			strv = g_strsplit (dep_raw, "(", 2);
			g_strstrip (strv[0]);

			li_pkg_info_set_name (ipkc, strv[0]);
			//ver_tmp = strv[1];
			//g_strstrip (ver_tmp);


			// TODO: Extract version and relation (>>, >=, <=, ==, <<)
		} else {
			li_pkg_info_set_name (ipkc, dep_raw);
		}

		g_ptr_array_add (array, ipkc);
	}

	return array;
}

/**
 * li_installed_dep_is_installed:
 */
gboolean
li_installer_dep_is_installed (GPtrArray *installed_sw, LiPkgInfo *ipkc)
{
	guint i;
	const gchar *dep_name;

	dep_name = li_pkg_info_get_name (ipkc);

	for (i = 0; i < installed_sw->len; i++) {
		const gchar *str;
		LiPkgInfo *pkg = LI_PKG_INFO (g_ptr_array_index (installed_sw, i));

		str = li_pkg_info_get_name (pkg);
		if (g_strcmp0 (dep_name, str) == 0) {
			return TRUE;
		}

		// TODO: Check version as well
	}

	return FALSE;
}

/**
 * li_installer_install_package:
 */
gboolean
li_installer_install_package (LiInstaller *inst, const gchar *filename, GError **error)
{
	LiIPKPackage *pkg;
	LiPkgInfo *info;
	GError *tmp_error;
	GPtrArray *deps = NULL;
	guint i;
	gboolean ret = FALSE;

	pkg = li_ipk_package_new ();
	li_ipk_package_open_file (pkg, filename, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	info = li_ipk_package_get_info (pkg);
	deps = li_installer_parse_dependency_string (li_pkg_info_get_dependencies (info));
	if (deps != NULL) {
		LiManager *mgr;
		GPtrArray *installed_sw;

		mgr = li_manager_new ();
		installed_sw = li_manager_get_installed_software (mgr);

		for (i = 0; i < deps->len; i++) {
			LiPkgInfo *dep = LI_PKG_INFO (g_ptr_array_index (deps, i));

			if (!li_installer_dep_is_installed (installed_sw, dep)) {
				g_set_error (error,
					LI_INSTALLER_ERROR,
					LI_INSTALLER_ERROR_DEPENDENCY_NOT_FOUND,
					_("Could not find dependency: %s"), li_pkg_info_get_name (dep));
				g_object_unref (mgr);
				goto out;
			}
		}
		g_object_unref (mgr);
	}

	li_ipk_package_install (pkg, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	ret = TRUE;
out:
	g_object_unref (pkg);
	if (deps != NULL)
		g_ptr_array_unref (deps);

	return ret;
}

/**
 * li_installer_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_installer_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiInstallerError");
	return quark;
}

/**
 * li_installer_class_init:
 **/
static void
li_installer_class_init (LiInstallerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_installer_finalize;
}

/**
 * li_installer_new:
 *
 * Creates a new #LiInstaller.
 *
 * Returns: (transfer full): a #LiInstaller
 *
 **/
LiInstaller *
li_installer_new (void)
{
	LiInstaller *inst;
	inst = g_object_new (LI_TYPE_INSTALLER, NULL);
	return LI_INSTALLER (inst);
}
