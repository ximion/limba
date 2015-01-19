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

#include "li-utils.h"
#include "li-utils-private.h"
#include "li-pkg-info.h"
#include "li-manager.h"
#include "li-runtime.h"
#include "li-package-graph.h"
#include "li-dbus-interface.h"

typedef struct _LiInstallerPrivate	LiInstallerPrivate;
struct _LiInstallerPrivate
{
	LiManager *mgr;
	LiPkgInfo *pki;
	LiPackageGraph *pg;
	LiPackage *pkg;

	gchar *fname;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiInstaller, li_installer, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_installer_get_instance_private (o))

static void li_installer_check_dependencies (LiInstaller *inst, GNode *root, GError **error);

/**
 * li_installer_finalize:
 **/
static void
li_installer_finalize (GObject *object)
{
	LiInstaller *inst = LI_INSTALLER (object);
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	g_object_unref (priv->mgr);
	g_object_unref (priv->pg);
	if (priv->pkg != NULL)
		g_object_unref (priv->pkg);
	g_free (priv->fname);

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
	priv->pg = li_package_graph_new ();
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
		LiPkgInfo *pki;

		dep_raw = slices[i];
		g_strstrip (dep_raw);

		pki = li_pkg_info_new ();
		if (g_strrstr (dep_raw, "(") != NULL) {
			gchar **strv;
			gchar *ver_tmp;

			strv = g_strsplit (dep_raw, "(", 2);
			g_strstrip (strv[0]);

			li_pkg_info_set_name (pki, strv[0]);
			ver_tmp = strv[1];
			g_strstrip (ver_tmp);
			if (strlen (ver_tmp) > 2) {
				LiVersionFlags flags = LI_VERSION_UNKNOWN;
				guint i;

				/* extract the version relation (>>, >=, <=, ==, <<) */
				for (i = 0; i <= 1; i++) {
					if (ver_tmp[i] == '>')
						flags |= LI_VERSION_HIGHER;
					else if (ver_tmp[i] == '<')
						flags |= LI_VERSION_LOWER;
					else if (ver_tmp[i] == '=')
						flags |= LI_VERSION_EQUAL;
					else {
						g_warning ("Found invalid character in version relation: %c", ver_tmp[i]);
						flags = LI_VERSION_UNKNOWN;
					}
				}

				/* extract the version */
				if (g_str_has_suffix (ver_tmp, ")")) {
					ver_tmp = g_strndup (ver_tmp+2, strlen (ver_tmp)-3);
					g_strstrip (ver_tmp);

					li_pkg_info_set_version (pki, ver_tmp);
					li_pkg_info_set_version_relation (pki, flags);
					g_free (ver_tmp);
				} else {
					g_warning ("Malformed dependency string found: Closing bracket of version is missing: %s (%s", strv[0], ver_tmp);
				}
			}
			g_strfreev (strv);
		} else {
			li_pkg_info_set_name (pki, dep_raw);
		}

		g_ptr_array_add (array, pki);
	}
	g_strfreev (slices);

	return array;
}

/**
 * li_installed_dep_is_installed:
 */
static LiPkgInfo*
li_installer_find_satisfying_pkg (GList *pkglist, LiPkgInfo *dep)
{
	GList *l;
	const gchar *dep_name;
	const gchar *dep_version;
	LiVersionFlags dep_vrel;

	if (pkglist == NULL)
		return NULL;

	dep_name = li_pkg_info_get_name (dep);
	dep_version = li_pkg_info_get_version (dep);
	dep_vrel = li_pkg_info_get_version_relation (dep);

	for (l = pkglist; l != NULL; l = l->next) {
		const gchar *pname;
		LiPkgInfo *pki = LI_PKG_INFO (l->data);

		pname = li_pkg_info_get_name (pki);
		if (g_strcmp0 (dep_name, pname) == 0) {
			gint cmp;
			const gchar *pver;
			/* we found something which has the same name as the software we are looking for */
			pver = li_pkg_info_get_version (pki);
			if (dep_version == NULL) {
				/* any version satisfies this dependency - so we are happy already */
				li_pkg_info_set_version (dep, pver);
				return pki;
			}

			/* now verify that its version is sufficient */
			cmp = li_compare_versions (pver, dep_version);
			if (((cmp == 1) && (dep_vrel & LI_VERSION_HIGHER)) ||
				((cmp == 0) && (dep_vrel & LI_VERSION_EQUAL)) ||
				((cmp == -1) && (dep_vrel & LI_VERSION_LOWER))) {
				/* we are good, the found package satisfies our requirements */

				/* update the version of the dependency to what we found */
				li_pkg_info_set_version (dep, pver);
				return pki;
			} else {
				g_debug ("Found %s (%s), skipping because version does not satisfy requirements(%i#%s).", pname, pver, dep_vrel, dep_version);
			}
		}
	}

	return NULL;
}

/**
 * li_installer_find_dependency_embedded_single:
 */
static gboolean
li_installer_find_dependency_embedded_single (LiInstaller *inst, GNode *root, LiPkgInfo *dep_pki, GError **error)
{
	LiPkgInfo *epki;
	LiPackage *epkg;
	GList *embedded;
	GError *tmp_error = NULL;
	GNode *node;
	LiPkgInfo *pki = LI_PKG_INFO (root->data);
	LiPackage *pkg;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	pkg = li_package_graph_get_install_candidate (priv->pg, pki);
	if (pkg == NULL) {
		g_debug ("Skipping dependency-lookup in installed package %s", li_pkg_info_get_id (pki));
		return FALSE;
	}

	embedded = li_package_get_embedded_packages (pkg);
	epki = li_installer_find_satisfying_pkg (embedded, dep_pki);
	if (epki == NULL) {
		g_set_error (error,
			LI_INSTALLER_ERROR,
			LI_INSTALLER_ERROR_DEPENDENCY_NOT_FOUND,
			_("Could not find dependency: %s"),
			li_pkg_info_get_name (dep_pki));
		return FALSE;
	}

	/* we have found a matching dependency! */
	epkg = li_package_graph_get_install_candidate (priv->pg, epki);
	if (epkg == NULL) {
		/* this package is not yet on the todo-list, so add it! */
		epkg = li_package_extract_embedded_package (pkg, epki, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}
	} else {
		g_object_ref (epkg);
	}

	node = li_package_graph_add_package_install_todo (priv->pg, root, epkg);

	/* check if we have the dependencies, or can install them */
	li_installer_check_dependencies (inst, node, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	g_object_unref (epkg);

	return TRUE;
}

/**
 * li_installer_find_dependency_embedded:
 *
 * This function checks for embedded dependencies in the current package,
 * the parent package and in the root package, which might satisfy dependencies
 * in the child.
 *
 * Returns: %TRUE if dependency was found and added to the graph
 */
static gboolean
li_installer_find_dependency_embedded (LiInstaller *inst, GNode *child, LiPkgInfo *dep, GError **error)
{
	GError *tmp_error = NULL;
	GNode *parent;
	GNode *root;
	gboolean ret;

	ret = li_installer_find_dependency_embedded_single (inst, child, dep, &tmp_error);
	if (ret)
		return TRUE;

	parent = child->parent;
	/* parent needs to be non-null (in case we hit root) */
	if (parent == NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	g_error_free (tmp_error);
	tmp_error = NULL;

	g_debug ("Reverse-lookup for component %s (~%s) in package %s (requested by %s).",
				li_pkg_info_get_name (dep),
				li_pkg_info_get_version (dep),
				li_pkg_info_get_id (LI_PKG_INFO (parent->data)),
				li_pkg_info_get_id (LI_PKG_INFO (child->data)));

	ret = li_installer_find_dependency_embedded_single (inst, parent, dep, &tmp_error);
	if (ret)
		return TRUE;

	root = g_node_get_root (parent);
	if (root == parent) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	g_error_free (tmp_error);
	tmp_error = NULL;

	g_debug ("Reverse-lookup for component %s (~%s) in root package.",
				li_pkg_info_get_name (dep),
				li_pkg_info_get_version (dep));

	ret = li_installer_find_dependency_embedded_single (inst, root, dep, &tmp_error);
	g_propagate_error (error, tmp_error);

	return ret;
}

/**
 * li_installer_check_dependencies:
 */
static void
li_installer_check_dependencies (LiInstaller *inst, GNode *root, GError **error)
{
	_cleanup_list_free_ GList *all_pkgs = NULL;
	GError *tmp_error = NULL;
	LiPackage *pkg;
	LiPkgInfo *pki;
	GPtrArray *deps;
	guint i;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	pki = LI_PKG_INFO (root->data);
	pkg = li_package_graph_get_install_candidate (priv->pg, pki);
	if (pkg == NULL)
		g_debug ("Hit installed package: %s", li_pkg_info_get_id (pki));
	else
		g_debug ("Hit new package: %s", li_pkg_info_get_id (pki));

	deps = li_installer_parse_dependency_string (li_pkg_info_get_dependencies (pki));

	/* do we have dependencies at all? */
	if (deps == NULL)
		return;

	all_pkgs = li_manager_get_software_list (priv->mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	for (i = 0; i < deps->len; i++) {
		LiPkgInfo *ipki;
		LiPkgInfo *dep = LI_PKG_INFO (g_ptr_array_index (deps, i));

		/* test if this package is already in the installed set */
		ipki = li_installer_find_satisfying_pkg (all_pkgs, dep);
		if ((ipki == NULL) || (!li_pkg_info_has_flag (ipki, LI_PACKAGE_FLAG_INSTALLED))) {
			/* maybe we find this dependency as embedded copy? */
			li_installer_find_dependency_embedded (inst, root, dep, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return;
			}
		} else {
			GNode *node;
			/* dependency is already installed, add it as satisfied */
			node = li_package_graph_add_package (priv->pg, root, ipki);

			/* we need a full dependency tree to generate one or more working runtimes later */
			li_installer_check_dependencies (inst, node, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return;
			}
		}
	}

	g_ptr_array_unref (deps);
}

/**
 * li_installer_install_node:
 */
gboolean
li_installer_install_node (LiInstaller *inst, GNode *node, GError **error)
{
	GError *tmp_error = NULL;
	GNode *child;
	LiPackage *pkg;
	LiPkgInfo *info;
	GPtrArray *full_deps = NULL;
	gboolean ret = FALSE;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	child = node->children;
	if (child != NULL) {
		while (TRUE) {
			li_installer_install_node (inst, child, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}

			child = child->next;
			if (child == NULL)
				break;
		}
	}

	/* already installed nodes are not really interesting here */
	info = LI_PKG_INFO (node->data);
	pkg = li_package_graph_get_install_candidate (priv->pg, info);
	if (pkg == NULL) {
		g_debug ("Skipping '%s': Already installed.", li_pkg_info_get_id (info));
		return TRUE;
	}

	/* only the root node was set for manual installation */
	if (!G_NODE_IS_ROOT (node))
		li_pkg_info_add_flag (info, LI_PACKAGE_FLAG_AUTOMATIC);

	/* now nstall the package */
	li_package_install (pkg, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	g_debug ("Installed package: %s", li_package_get_id (pkg));
	li_package_graph_mark_installed (priv->pg, info);

	/* create runtime for this software, if one is required */
	full_deps = li_package_graph_branch_to_array (node);
	if ((li_pkg_info_get_flags (info) & LI_PACKAGE_FLAG_APPLICATION) && (full_deps != NULL)) {
		LiRuntime *rt;

		/* now get the runtime-env id for the new application */
		rt = li_manager_find_runtime_with_members (priv->mgr, full_deps);
		if (rt == NULL) {
			g_debug ("Creating new runtime for %s.", li_package_get_id (pkg));
			/* no runtime was found, create a new one */
			rt = li_runtime_create_with_members (full_deps, &tmp_error);
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
	/* store the changed metadata on disk */
	li_pkg_info_save_changes (info);

	ret = TRUE;
out:
	if (full_deps != NULL)
		g_ptr_array_unref (full_deps);

	return ret;
}

/**
 * li_installer_set_package:
 */
static void
li_installer_set_package (LiInstaller *inst, LiPackage *pkg)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	li_package_graph_set_root_install_todo (priv->pg, pkg);

	/* we hold a reference to our main package over the whole LiInstaller lifecycle */
	if (priv->pkg != NULL)
		g_object_unref (priv->pkg);
	priv->pkg = g_object_ref (pkg);
}

/**
 * li_installer_install:
 */
gboolean
li_installer_install (LiInstaller *inst, GError **error)
{
	gboolean ret = FALSE;
	GError *tmp_error = NULL;
	GNode *root;
	LimbaInstaller *inst_bus = NULL;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	if (!li_utils_is_root ()) {
		/* we do not have root privileges - call the helper daemon to install the package */
		g_debug ("Calling Limba DBus service.");

		inst_bus = limba_installer_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
											G_DBUS_PROXY_FLAGS_NONE,
											"org.freedesktop.Limba",
											"/org/freedesktop/Limba/Installer",
											NULL,
											&tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

		limba_installer_call_local_install_sync (inst_bus, priv->fname, NULL, &tmp_error);

		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

		goto out;
	}

	root = li_package_graph_get_root (priv->pg);
	if (root->data == NULL) {
		g_set_error (error,
			LI_INSTALLER_ERROR,
			LI_INSTALLER_ERROR_FAILED,
			_("No package is loaded."));
		return FALSE;
	}

	/* create a dependency tree for this package installation */
	/* NOTE: The root node is always the to-be-installed package (a LiPackage instance) */
	li_installer_check_dependencies (inst, root, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	/* install the package tree */
	ret = li_installer_install_node (inst, root, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

out:
	/* teardown current dependency tree */
	li_package_graph_reset (priv->pg);
	if (inst_bus != NULL)
		g_object_unref (inst_bus);

	return ret;
}

/**
 * li_installer_open_file:
 *
 * Open a package file
 */
gboolean
li_installer_open_file (LiInstaller *inst, const gchar *filename, GError **error)
{
	LiPackage *pkg;
	GError *tmp_error = NULL;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	pkg = li_package_new ();
	li_package_open_file (pkg, filename, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_object_unref (pkg);
		return FALSE;
	}
	li_installer_set_package (inst, pkg);
	g_object_unref (pkg);

	/* set filename, in case we need it for a DBus call later */
	if (priv->fname != NULL)
		g_free (priv->fname);
	priv->fname = g_strdup (filename);

	return TRUE;
}

/**
 * li_installer_get_package_info:
 *
 * Returns: (transfer none): The #LiPkgInfo of the to-be-installed package
 */
LiPkgInfo*
li_installer_get_package_info (LiInstaller *inst)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);
	return li_package_get_info (priv->pkg);
}

/**
 * li_installer_get_package_trust_level:
 *
 * Returns: The trust-level for the to-be-installed package
 */
LiTrustLevel
li_installer_get_package_trust_level (LiInstaller *inst)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);
	return	li_package_verify_signature (priv->pkg, NULL);
}

/**
 * li_installer_get_appstream_data:
 *
 * Dump of AppStream XML data describing the software which will be installed.
 *
 * Returns: (transfer full): AppStream XML data or %NULL, free with g_free()
 */
gchar*
li_installer_get_appstream_data (LiInstaller *inst)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	if (priv->pkg == NULL)
		return NULL;

	return li_package_get_appstream_data (priv->pkg);
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
