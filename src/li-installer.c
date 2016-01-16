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
#include "li-pkg-cache.h"
#include "li-config-data.h"
#include "li-dbus-interface.h"

typedef struct _LiInstallerPrivate	LiInstallerPrivate;
struct _LiInstallerPrivate
{
	LiManager *mgr;
	LiPkgInfo *pki;
	LiPackageGraph *pg;
	LiPackage *pkg;
	LiPkgCache *cache;
	GPtrArray *all_pkgs;

	gchar *fname;
	GMainLoop *loop;
	GError *proxy_error;
	LiProxyManager *bus_proxy;
	guint bus_watch_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiInstaller, li_installer, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (li_installer_get_instance_private (o))

enum {
	SIGNAL_STAGE_CHANGED,
	SIGNAL_PROGRESS,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

static void li_installer_check_dependencies (LiInstaller *inst, GNode *root, GError **error);
static void li_installer_package_graph_progress_cb (LiPackageGraph *pg, guint percentage, const gchar *id, LiInstaller *inst);
static void li_installer_package_graph_stage_changed_cb (LiPackageGraph *pg, LiPackageStage stage, const gchar *id, LiInstaller *inst);

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
	g_object_unref (priv->cache);
	if (priv->all_pkgs != NULL)
		g_ptr_array_unref (priv->all_pkgs);
	if (priv->pkg != NULL)
		g_object_unref (priv->pkg);
	g_free (priv->fname);
	g_main_loop_unref (priv->loop);
	if (priv->proxy_error != NULL)
		g_error_free (priv->proxy_error);
	if (priv->bus_proxy != NULL)
		g_object_unref (priv->bus_proxy);
	if (priv->bus_watch_id != 0)
		g_bus_unwatch_name (priv->bus_watch_id);

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
	priv->cache = li_pkg_cache_new ();
	priv->loop = g_main_loop_new (NULL, FALSE);

	/* connect signals */
	g_signal_connect (priv->pg, "progress",
				G_CALLBACK (li_installer_package_graph_progress_cb), inst);
	g_signal_connect (priv->pg, "stage-changed",
				G_CALLBACK (li_installer_package_graph_stage_changed_cb), inst);
}

/**
 * li_installer_package_graph_progress_cb:
 */
static void
li_installer_package_graph_progress_cb (LiPackageGraph *pg, guint percentage, const gchar *id, LiInstaller *inst)
{
	/* just forward that stuff */
	g_signal_emit (inst, signals[SIGNAL_PROGRESS], 0,
					percentage, id);
}

/**
 * li_installer_package_graph_stage_changed_cb:
 */
static void
li_installer_package_graph_stage_changed_cb (LiPackageGraph *pg, LiPackageStage stage, const gchar *id, LiInstaller *inst)
{
	/* just forward that stuff */
	g_signal_emit (inst, signals[SIGNAL_STAGE_CHANGED], 0,
					stage, id);
}

/**
 * li_installer_add_dependency_remote:
 */
static gboolean
li_installer_add_dependency_remote (LiInstaller *inst, GNode *root, LiPkgInfo *dep_pki, GError **error)
{
	GNode *node;
	LiPackage *pkg;
	GError *tmp_error = NULL;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	pkg = li_package_new ();
	li_package_open_remote (pkg, priv->cache, li_pkg_info_get_id (dep_pki), &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_object_unref (pkg);
		return FALSE;
	}

	node = li_package_graph_add_package_install_todo (priv->pg, root, pkg, dep_pki);

	/* check if we have the dependencies, or can install them */
	li_installer_check_dependencies (inst, node, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	g_object_unref (pkg);

	return TRUE;
}

/**
 * li_installer_find_dependency_embedded_single:
 */
static gboolean
li_installer_find_dependency_embedded_single (LiInstaller *inst, GNode *root, LiPkgInfo *dep_pki, GError **error)
{
	LiPkgInfo *epki;
	LiPackage *epkg;
	GPtrArray *embedded;
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
	epki = li_find_satisfying_pkg (embedded, dep_pki);
	if (epki == NULL) {
		/* embedded packages were our last chance - we give up */
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

	node = li_package_graph_add_package_install_todo (priv->pg, root, epkg, dep_pki);

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
	GError *tmp_error = NULL;
	LiPackage *pkg;
	LiPkgInfo *pki;
	g_autoptr(GPtrArray) deps = NULL;
	guint i;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	pki = LI_PKG_INFO (root->data);
	pkg = li_package_graph_get_install_candidate (priv->pg, pki);
	if (pkg == NULL)
		g_debug ("Hit installed package: %s", li_pkg_info_get_id (pki));
	else
		g_debug ("Hit new package: %s", li_pkg_info_get_id (pki));

	deps = li_parse_dependencies_string (li_pkg_info_get_dependencies (pki));

	/* do we have dependencies at all? */
	if (deps == NULL)
		return;

	if (priv->all_pkgs == NULL) {
		priv->all_pkgs = li_manager_get_software_list (priv->mgr, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}
	}

	for (i = 0; i < deps->len; i++) {
		LiPkgInfo *ipki;
		gboolean ret;
		LiPkgInfo *dep = LI_PKG_INFO (g_ptr_array_index (deps, i));

		/* test if we have a dependency on a system component */
		ret = li_package_graph_test_foundation_dependency (priv->pg, dep, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}
		/* continue if dependency is already satisfied */
		if (ret)
			continue;

		/* test if this package is already in the installed set */
		ipki = li_find_satisfying_pkg (priv->all_pkgs, dep);
		if (ipki == NULL) {
			/* maybe we find this dependency as embedded copy? */
			li_installer_find_dependency_embedded (inst, root, dep, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return;
			}
		} else if (li_pkg_info_has_flag (ipki, LI_PACKAGE_FLAG_AVAILABLE)) {
			g_debug ("Hit remote package: %s", li_pkg_info_get_id (ipki));

			li_installer_add_dependency_remote (inst, root, dep, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return;
			}
		} else {
			GNode *node;

			if (!li_pkg_info_has_flag (ipki, LI_PACKAGE_FLAG_INSTALLED))
				g_warning ("Found package '%s' which should be in INSTALLED state, but actually is not. Ignoring issue and assuming INSTALLED.",
							li_pkg_info_get_id (ipki));

			/* dependency is already installed, add it as satisfied */
			node = li_package_graph_add_package (priv->pg, root, ipki, dep);

			/* we need a full dependency tree to generate one or more working runtimes later */
			li_installer_check_dependencies (inst, node, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return;
			}
		}
	}
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

	/* now install the package */
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
 * li_installer_proxy_error_cb:
 *
 * Callback for the Error() DBus signal
 */
static void
li_installer_proxy_error_cb (LiProxyManager *mgr_bus, guint32 domain, guint code, const gchar *message, LiInstaller *inst)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	/* ensure no error is set */
	if (priv->proxy_error != NULL) {
		g_error_free (priv->proxy_error);
		priv->proxy_error = NULL;
	}

	g_set_error (&priv->proxy_error, domain, code, "%s", message);
}

/**
 * li_installer_proxy_finished_cb:
 *
 * Callback for the Finished() DBus signal
 */
static void
li_installer_proxy_finished_cb (LiProxyManager *mgr_bus, gboolean success, LiInstaller *inst)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	if (success) {
		/* ensure no error is set */
		if (priv->proxy_error != NULL) {
			g_error_free (priv->proxy_error);
			priv->proxy_error = NULL;
		}
	}

	g_main_loop_quit (priv->loop);
}

/**
 * li_installer_proxy_progress_cb:
 */
static void
li_installer_proxy_progress_cb (LiProxyManager *mgr_bus, const gchar *id, gint percentage, LiInstaller *inst)
{
	if (g_strcmp0 (id, "") == 0)
		id = NULL;

	g_signal_emit (inst, signals[SIGNAL_PROGRESS], 0,
					percentage, id);
}

/**
 * li_installer_bus_vanished:
 */
static void
li_installer_bus_vanished (GDBusConnection *connection, const gchar *name, LiInstaller *inst)
{
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	/* check if we are actually running any action */
	if (!g_main_loop_is_running (priv->loop))
		return;

	if (priv->proxy_error == NULL) {
		/* set error, since the DBus name vanished while performing an action, indicating that something crashed */
		g_set_error (&priv->proxy_error,
			LI_INSTALLER_ERROR,
			LI_INSTALLER_ERROR_INTERNAL,
			_("The Limba daemon vanished from the bus mid-transaction, so it likely crashed. Please file a bug against Limba."));
	}

	g_main_loop_quit (priv->loop);
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
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	if (!li_utils_is_root ()) {
		/* we do not have root privileges - call the helper daemon to install the package */
		g_debug ("Calling Limba DBus service.");

		if (priv->bus_proxy == NULL) {
			/* looks like we do not yet have a bus connection, so we create one */
			priv->bus_proxy = li_proxy_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
											G_DBUS_PROXY_FLAGS_NONE,
											"org.freedesktop.Limba",
											"/org/freedesktop/Limba/Manager",
											NULL,
											&tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}

			/* we want to know when the bus name vanishes unexpectedly */
			if (priv->bus_watch_id == 0) {
				priv->bus_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
									"org.freedesktop.Limba",
									G_BUS_NAME_WATCHER_FLAGS_NONE,
									NULL,
									(GBusNameVanishedCallback) li_installer_bus_vanished,
									inst,
									NULL);
			}

			g_signal_connect (priv->bus_proxy, "progress",
						G_CALLBACK (li_installer_proxy_progress_cb), inst);
			g_signal_connect (priv->bus_proxy, "error",
						G_CALLBACK (li_installer_proxy_error_cb), inst);
			g_signal_connect (priv->bus_proxy, "finished",
						G_CALLBACK (li_installer_proxy_finished_cb), inst);
		}

		/* ensure no error is set */
		if (priv->proxy_error != NULL) {
			g_error_free (priv->proxy_error);
			priv->proxy_error = NULL;
		}

		if (priv->fname != NULL) {
			/* we install a local package, so call the respective DBus method */
			li_proxy_manager_call_install_local_sync (priv->bus_proxy,
							priv->fname,
							NULL,
							&tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
		} else {
			/* we install package from a repository */
			li_proxy_manager_call_install_sync (priv->bus_proxy,
							li_package_get_id (priv->pkg),
							NULL,
							&tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
		}

		/* if we are here, we can wait for a Finished() signal */
		g_main_loop_run (priv->loop);

		if (priv->proxy_error != NULL) {
			g_propagate_error (error, priv->proxy_error);
			priv->proxy_error = NULL;
			goto out;
		}

		goto out;
	}

	/* ensure the graph is initialized and additional data (foundations list) is loaded */
	li_package_graph_initialize (priv->pg, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
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

	/* open the package cache */
	li_pkg_cache_open (priv->cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
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

	return ret;
}

/**
 * li_installer_open_file:
 * @filename: The local IPK package filename
 *
 * Open a package file for installation.
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

	/* ensure we update the list of known packages */
	if (priv->all_pkgs != NULL)
		g_ptr_array_unref (priv->all_pkgs);
	priv->all_pkgs = NULL;

	return TRUE;
}

/**
 * li_installer_open_remote:
 * @pkgid: The package/bundle-id of the software to install.
 *
 * Install software from a repository.
 */
gboolean
li_installer_open_remote (LiInstaller *inst, const gchar *pkgid, GError **error)
{
	g_autoptr(LiPackage) pkg = NULL;
	GError *tmp_error = NULL;
	LiInstallerPrivate *priv = GET_PRIVATE (inst);

	/* open the package cache */
	li_pkg_cache_open (priv->cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	pkg = li_package_new ();
	li_package_open_remote (pkg, priv->cache, pkgid, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* when downloading packages from the cache, we already verified the index file,
	 * and after download we also verify the SHA256 checksums of the index - no need
	 * to also verify the internal signature of the IPK package (especially if we may
	 * not have its public-key in the keyring). */
	li_package_set_auto_verify (pkg, FALSE);

	li_installer_set_package (inst, pkg);

	/* ensure we update the list of known packages */
	if (priv->all_pkgs != NULL)
		g_ptr_array_unref (priv->all_pkgs);
	priv->all_pkgs = NULL;

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
	return li_package_verify_signature (priv->pkg, NULL);
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

	signals[SIGNAL_PROGRESS] =
		g_signal_new ("progress",
				G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
				0, NULL, NULL, g_cclosure_marshal_VOID__UINT_POINTER,
				G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);

	signals[SIGNAL_STAGE_CHANGED] =
		g_signal_new ("stage-changed",
				G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
				0, NULL, NULL, g_cclosure_marshal_VOID__UINT_POINTER,
				G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
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
