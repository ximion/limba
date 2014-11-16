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
#include "li-manager.h"
#include "li-runtime.h"

typedef struct _LiInstallerPrivate	LiInstallerPrivate;
struct _LiInstallerPrivate
{
	LiManager *mgr;
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

	g_object_unref (priv->mgr);

	G_OBJECT_CLASS (li_installer_parent_class)->finalize (object);
}

/**
 * li_installer_init:
 **/
static void
li_installer_init (LiInstaller *inst)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	priv->mgr = li_manager_new ();
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
static LiPkgInfo*
li_installer_find_satisfying_pkg (GPtrArray *pkglist, LiPkgInfo *dep)
{
	guint i;
	const gchar *dep_name;

	if (pkglist == NULL)
		return NULL;

	dep_name = li_pkg_info_get_name (dep);
	for (i = 0; i < pkglist->len; i++) {
		const gchar *str;
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (pkglist, i));

		str = li_pkg_info_get_name (pki);
		if (g_strcmp0 (dep_name, str) == 0) {
			/* update version of the dependency to match the one of the installed software */
			str = li_pkg_info_get_version (pki);
			li_pkg_info_set_version (dep, str);
			return pki;
		}

		// TODO: Check version as well

	}

	return NULL;
}

/**
 * li_installer_install_dependency_from_embedded:
 */
static gboolean
li_installer_install_dependency_from_embedded (LiInstaller *inst, LiPackage *pkg, LiPkgInfo *dep_pki, GError **error)
{
	LiPkgInfo *epki;
	LiPackage *epkg;
	LiInstaller *einst;
	GPtrArray *embedded;
	GError *tmp_error = NULL;

	embedded = li_package_get_embedded_packages (pkg);
	epki = li_installer_find_satisfying_pkg (embedded, dep_pki);
	if (epki == NULL) {
		g_set_error (error,
			LI_INSTALLER_ERROR,
			LI_INSTALLER_ERROR_DEPENDENCY_NOT_FOUND,
			_("Could not find dependency: %s"), li_pkg_info_get_name (dep_pki));
		return FALSE;
	}

	/* we have found a matching dependency! */
	epkg = li_package_extract_embedded_package (pkg, epki, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* install dependency */
	einst = li_installer_new ();
	li_installer_install_package (einst, epkg, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

/**
 * li_installer_install_package:
 */
gboolean
li_installer_install_package (LiInstaller *inst, LiPackage *pkg, GError **error)
{
	LiPkgInfo *info;
	GError *tmp_error = NULL;
	GPtrArray *deps = NULL;
	guint i;
	gboolean ret = FALSE;
	LiRuntime *rt;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	info = li_package_get_info (pkg);
	deps = li_installer_parse_dependency_string (li_pkg_info_get_dependencies (info));
	if (deps != NULL) {
		GPtrArray *installed_sw;

		installed_sw = li_manager_get_installed_software (priv->mgr);

		for (i = 0; i < deps->len; i++) {
			LiPkgInfo *dep = LI_PKG_INFO (g_ptr_array_index (deps, i));

			if (li_installer_find_satisfying_pkg (installed_sw, dep) == NULL) {
				/* maybe we find this dependency as embedded copy? */
				li_installer_install_dependency_from_embedded (inst, pkg, dep, &tmp_error);
				if (tmp_error != NULL) {
					g_propagate_error (error, tmp_error);
					goto out;
				}
			}
		}

		/* now get the runtime-env id for the new application */
		rt = li_manager_find_runtime_with_members (priv->mgr, deps);
		if (rt == NULL) {
			/* no runtime was found, create a new one */
			rt = li_runtime_create_with_members (deps, &tmp_error);
			if ((tmp_error != NULL) || (rt == NULL)) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
		}
		li_pkg_info_set_runtime_dependency (info,
										li_runtime_get_uuid (rt));
		g_object_unref (rt);
	} else {
		/* if the installed software does not need a runtime to run, we explicity state that */
		li_pkg_info_set_runtime_dependency (info, "None");
	}

	li_package_install (pkg, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	ret = TRUE;
out:
	if (deps != NULL)
		g_ptr_array_unref (deps);

	return ret;
}

/**
 * li_installer_install_package_file:
 */
gboolean
li_installer_install_package_file (LiInstaller *inst, const gchar *filename, GError **error)
{
	LiPackage *pkg;
	GError *tmp_error = NULL;
	gboolean ret;

	pkg = li_package_new ();
	li_package_open_file (pkg, filename, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_object_unref (pkg);
		return FALSE;
	}

	ret = li_installer_install_package (inst, pkg, &tmp_error);
	g_object_unref (pkg);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

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
