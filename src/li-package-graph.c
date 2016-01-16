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
 * SECTION:li-package-graph
 * @short_description: Represents a package dependency graph
 *
 * This class represents a graph of packages (nodes are #LiPkgInfo instances, edges represent a dependency),
 * as well as a hash-table of packages which need installation.
 * It is mainly used by #LiInstaller at time.
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include <math.h>
#include "li-package-graph.h"

#include "li-utils.h"
#include "li-config-data.h"
#include "li-installer.h"


typedef struct _LiPackageGraphPrivate	LiPackageGraphPrivate;
struct _LiPackageGraphPrivate
{
	GPtrArray *alist;
	GHashTable *nindex;
	GHashTable *install_todo;

	guint progress;
	guint max_progress;

	GHashTable *foundations;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPackageGraph, li_package_graph, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (li_package_graph_get_instance_private (o))

enum {
	SIGNAL_STAGE_CHANGED,
	SIGNAL_PROGRESS,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

/**
 * li_package_graph_finalize:
 **/
static void
li_package_graph_finalize (GObject *object)
{
	LiPackageGraph *pg = LI_PACKAGE_GRAPH (object);
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	g_ptr_array_unref (priv->alist);
	g_hash_table_unref (priv->nindex);
	g_hash_table_unref (priv->install_todo);
	g_hash_table_unref (priv->foundations);

	G_OBJECT_CLASS (li_package_graph_parent_class)->finalize (object);
}

/**
 * li_package_graph_init:
 **/
static void
li_package_graph_init (LiPackageGraph *pg)
{
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	priv->alist = g_ptr_array_new_with_free_func ((GDestroyNotify) g_ptr_array_unref);
	priv->nindex = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	priv->install_todo = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->foundations = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

/**
 * li_package_graph_update_foundations_table:
 */
static void
li_package_graph_update_foundations_table (LiPackageGraph *pg, GError **error)
{
	g_autoptr(LiConfigData) fdconf = NULL;
	GError *tmp_error = NULL;
	GFile *file;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	if (g_hash_table_size (priv->foundations) > 0)
		return;

	fdconf = li_config_data_new ();
	file = g_file_new_for_path (DATADIR "/foundations.list");
	if (g_file_query_exists (file, NULL)) {
		li_config_data_load_file (fdconf, file, &tmp_error);
	} else {
		g_warning ("No foundation (system-component) was defined. Continuing without that knowledge.");
	}
	g_object_unref (file);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	li_config_data_reset (fdconf);

	do {
		g_autofree gchar *fid = NULL;
		gchar *condition;
		gboolean ret;
		fid = li_config_data_get_value (fdconf, "ID");

		/* skip invalid data */
		if (fid == NULL)
			continue;

		condition = li_config_data_get_value (fdconf, "ConditionFileExists");
		if (condition != NULL) {
			ret = g_file_test (condition, G_FILE_TEST_IS_REGULAR);
			/* skip foundation if condition was not satisfied */
			if (!ret) {
				g_debug ("Foundation '%s' is not installed.", fid);
				continue;
			}
		}

		/* TODO: Implement ConditionLibraryExists */

		/* TODO: Create a LiPkgInfo for each foundation, to produce better (error) messages later, and
		 * to be more verbose */
		g_hash_table_insert (priv->foundations,
					g_strdup (fid),
					g_strdup (fid));

	} while (li_config_data_next (fdconf));
}

/**
 * li_package_graph_initialize:
 *
 * Load additional data to aid the resolving process.
 */
void
li_package_graph_initialize (LiPackageGraph *pg, GError **error)
{
	GError *tmp_error = NULL;

	/* populate the foundations registry, if not yet done */
	li_package_graph_update_foundations_table (pg, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_prefixed_error (error, tmp_error,
						"Could not load foundations list.");
		return;
	}
}

/**
 * li_package_graph_package_progress_cb:
 */
static void
li_package_graph_package_progress_cb (LiPackage *pkg, guint percentage, LiPackageGraph *pg)
{
	guint main_percentage;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	main_percentage = round (100 / (double) priv->max_progress * (priv->progress+percentage));

	/* emit individual progress */
	g_signal_emit (pg, signals[SIGNAL_PROGRESS], 0,
			percentage, li_package_get_id (pkg));

	/* emit main progress */
	g_signal_emit (pg, signals[SIGNAL_PROGRESS], 0,
			main_percentage, NULL);
}

/**
 * li_package_graph_package_stage_changed_cb:
 */
static void
li_package_graph_package_stage_changed_cb (LiPackage *pkg, LiPackageStage stage, LiPackageGraph *pg)
{
	/* forward the signal, with the package-id attached */
	g_signal_emit (pg, signals[SIGNAL_STAGE_CHANGED], 0,
			stage, li_package_get_id (pkg));
}

/**
 * li_package_graph_add_package:
 * @pki: The information about an installed package
 * @satisfied_dep: The dependency this package satisfies, or %NULL
 *
 * Returns: A reference to the new row of the
 */
GPtrArray*
li_package_graph_add_package (LiPackageGraph *pg, LiPkgInfo *parent, LiPkgInfo *pki, LiPkgInfo *satisfied_dep)
{
	GPtrArray *row;
	GPtrArray *parent_row;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	row = g_hash_table_lookup (priv->nindex, li_pkg_info_get_id (pki));
	if (row == NULL) {
		row = g_ptr_array_new_with_free_func (g_object_unref);
		g_ptr_array_add (row, g_object_ref (pki));
		g_ptr_array_add (priv->alist, row);
		g_hash_table_insert (priv->nindex,
					g_strdup (li_pkg_info_get_id (pki)),
					row);
		g_debug ("Added: %s/%s", li_pkg_info_get_name (pki), li_pkg_info_get_version (pki));
	}

	if (parent != NULL) {
		parent_row = g_hash_table_lookup (priv->nindex, li_pkg_info_get_id (parent));
		if (parent_row == NULL) {
			g_warning ("Tried to add %s to invalid parent.", li_pkg_info_get_id (pki));
		} else {
			g_ptr_array_add (parent_row, g_object_ref (pki));
		}
	}

	if (satisfied_dep != NULL)
		li_pkg_info_set_version_relation (pki,
						  li_pkg_info_get_version_relation (satisfied_dep));

	return row;
}

/**
 * li_package_graph_add_package_install_todo:
 * @pkg: The package required to be installed
 * @satisfied_dep: The dependency this package satisfies, or %NULL
 *
 * Returns: A reference to the new node
 */
GPtrArray*
li_package_graph_add_package_install_todo (LiPackageGraph *pg, LiPkgInfo *parent, LiPackage *pkg, LiPkgInfo *satisfied_dep)
{
	GPtrArray *row;
	gboolean ret;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	row = li_package_graph_add_package (pg,
					     parent,
					     li_package_get_info (pkg),
					     satisfied_dep);

	ret = g_hash_table_insert (priv->install_todo,
					g_strdup (li_package_get_id (pkg)),
					g_object_ref (pkg));
	if (ret) {
		g_debug ("Package %s marked for installation.", li_package_get_id (pkg));

		/* connect signals */
		g_signal_connect (pkg, "progress",
					G_CALLBACK (li_package_graph_package_progress_cb), pg);
		g_signal_connect (pkg, "stage-changed",
					G_CALLBACK (li_package_graph_package_stage_changed_cb), pg);
	} else {
		g_debug ("Package %s already marked for installation.", li_package_get_id (pkg));
	}

	priv->max_progress = g_hash_table_size (priv->install_todo)*100;

	return row;
}

/**
 * li_package_graph_pkg_is_required:
 *
 * Check if the given package has other packages depending on it.
 */
gboolean
li_package_graph_pkg_is_required (LiPackageGraph *pg, LiPkgInfo *pki)
{
	guint i;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	for (i = 0; i < priv->alist->len; i++) {
		guint j;
		GPtrArray *row = (GPtrArray*) g_ptr_array_index (priv->alist, i);

		for (j = 0; j < row->len; j++) {
			LiPkgInfo *pki2 = LI_PKG_INFO (g_ptr_array_index (row, j));
			if (pki == pki2)
				return TRUE;
		}
	}

	return FALSE;
}

/**
 * li_package_graph_get_install_candidate:
 */
LiPackage*
li_package_graph_get_install_candidate (LiPackageGraph *pg, LiPkgInfo *pki)
{
	LiPackage *pkg;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	pkg = g_hash_table_lookup (priv->install_todo,
					li_pkg_info_get_id (pki));

	if (pkg != NULL)
		li_pkg_info_set_version_relation (li_package_get_info (pkg),
						  li_pkg_info_get_version_relation (pki));

	return pkg;
}

/**
 * li_package_graph_mark_installed:
 */
gboolean
li_package_graph_mark_installed (LiPackageGraph *pg, LiPkgInfo *pki)
{
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	priv->progress += 100;
	return g_hash_table_remove (priv->install_todo,
					li_pkg_info_get_id (pki));
}

/**
 * li_package_graph_branch_to_array:
 *
 * Get an array of #LiPkgInfo objects this node depends on.
 */
GPtrArray*
li_package_graph_branch_to_array (LiPackageGraph *pg, LiPkgInfo *root, gboolean include_root)
{
	guint i;
	GPtrArray *arow;
	GPtrArray *array;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	arow = g_hash_table_lookup (priv->nindex, li_pkg_info_get_id (root));
	if (arow == NULL)
		return NULL;

	array = g_ptr_array_new ();
	/* if the row only contains itself, the pkg doesn't depend on anything */
	if (!include_root) {
		if (arow->len <= 1)
			return array;
	}

	for (i = 0; i < arow->len; i++) {
		g_ptr_array_add (array,
				 g_ptr_array_index (arow, i));
	}

	if (!include_root)
		g_ptr_array_remove (array, root);

	return array;
}

/**
 * li_package_graph_node_get_any_parent_manual:
 *
 * Check if a node has children containing packages which were installed
 * manually.
 */
gboolean
li_package_graph_node_get_any_parent_manual (LiPackageGraph *pg, LiPkgInfo *root)
{
	guint i;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	for (i = 0; i < priv->alist->len; i++) {
		guint j;
		LiPkgInfo *row_pki;
		GPtrArray *row =  (GPtrArray*) g_ptr_array_index (priv->alist, i);

		row_pki = LI_PKG_INFO (g_ptr_array_index (row, 0));
		for (j = 1; j < row->len; j++) {
			LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (row, j));
			if (pki == root) {
				if (!li_pkg_info_has_flag (row_pki, LI_PACKAGE_FLAG_AUTOMATIC)) {
					return TRUE;
				}
				break;
			}
		}
	}

	return FALSE;
}

/**
 * li_package_graph_reset:
 *
 * Remove all nodes from the graph, except for the root node.
 */
void
li_package_graph_reset (LiPackageGraph *pg)
{
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	g_ptr_array_unref (priv->alist);
	g_hash_table_unref (priv->nindex);
	g_hash_table_unref (priv->install_todo);

	priv->alist = g_ptr_array_new_with_free_func ((GDestroyNotify) g_ptr_array_unref);
	priv->nindex = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	priv->install_todo = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

/**
 * li_package_graph_get_install_todo_count:
 *
 * Get the number of packages which need to be installed.
 */
guint
li_package_graph_get_install_todo_count (LiPackageGraph *pg)
{
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);
	return g_hash_table_size (priv->install_todo);
}

/**
 * li_package_graph_test_foundation_dependency:
 *
 * Check if we have a foundation dependency.
 *
 * Returns: %TRUE if dependency is satisfied, and %FALSE if it is not.
 * In case we have failed to find the dependency, error is set.
 */
gboolean
li_package_graph_test_foundation_dependency (LiPackageGraph *pg, LiPkgInfo *dep_pki, GError **error)
{
	const gchar *pkname;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	pkname = li_pkg_info_get_name (dep_pki);

	/* check if this dependency is a foundation dependency */
	if (!g_str_has_prefix (pkname, "foundation:"))
		return FALSE;

	if (g_hash_table_lookup (priv->foundations, pkname) != NULL) {
		/* foundation was found, dependency is satisfied! */
		g_debug ("Detected system dependency '%s' as satisfied.", pkname);
		return TRUE;
	} else {
		g_set_error (error,
			LI_INSTALLER_ERROR,
			LI_INSTALLER_ERROR_FOUNDATION_NOT_FOUND,
			_("Could not find system component: '%s'. Please install it manually."), pkname);
		return FALSE;
	}
}

/**
 * li_find_satisfying_pkg:
 *
 * Return a package from the given package-list which matches the criteria
 * defined in dep.
 *
 * TODO: Return the highest version of a package matching the constraints.
 */
LiPkgInfo*
li_find_satisfying_pkg (GPtrArray *pkglist, LiPkgInfo *dep)
{
	guint i;
	const gchar *dep_name;
	const gchar *dep_version;
	LiVersionFlags dep_vrel;
	LiPkgInfo *res_pki = NULL;

	if (pkglist == NULL)
		return NULL;

	dep_name = li_pkg_info_get_name (dep);
	dep_version = li_pkg_info_get_version (dep);
	dep_vrel = li_pkg_info_get_version_relation (dep);

	for (i = 0; i < pkglist->len; i++) {
		const gchar *pname;
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (pkglist, i));

		pname = li_pkg_info_get_name (pki);
		if (g_strcmp0 (dep_name, pname) == 0) {
			gint cmp;
			const gchar *pver;
			/* we found something which has the same name as the software we are looking for */
			pver = li_pkg_info_get_version (pki);
			if (dep_version == NULL) {
				/* any version satisfies this dependency - so we are happy already */
				res_pki = pki;
				goto out;
			}

			/* now verify that its version is sufficient */
			cmp = li_compare_versions (pver, dep_version);
			if (((cmp == 1) && (dep_vrel & LI_VERSION_HIGHER)) ||
				((cmp == 0) && (dep_vrel & LI_VERSION_EQUAL)) ||
				((cmp == -1) && (dep_vrel & LI_VERSION_LOWER))) {
				/* we are good, the found package satisfies our requirements */

				res_pki = pki;
				goto out;
			} else {
				g_debug ("Found %s (%s), skipping because version does not satisfy requirements(%i#%s).",
						pname, pver, dep_vrel, dep_version);
			}
		}
	}

out:
	if (res_pki != NULL) {
		/* update the version of the dependency to what we found */
		li_pkg_info_set_version (dep,
					 li_pkg_info_get_version (res_pki));

		/* update the version restrictions of the found package - kind of hackish to do it here, but very convenient */
		li_pkg_info_set_version_relation (res_pki, dep_vrel);
	}

	return res_pki;
}

/**
 * li_package_graph_new_from_pkiarray:
 *
 * Create a new package graph and populate it with data from an
 * array of #LiPkgInfo objects.
 */
LiPackageGraph*
li_package_graph_new_from_pkiarray (GPtrArray *pkiarray, GError **error)
{
	guint i;
	LiPackageGraph *pg;
	LiPackageGraphPrivate *priv;

	pg = li_package_graph_new ();
	priv = GET_PRIVATE (pg);

	/* check if there is something to do */
	if (pkiarray->len == 0)
		return pg;

	/* build index of all nodes and add nodes (without any edges) to the adjacency list */
	for (i = 0; i < pkiarray->len; i++) {
		LiPkgInfo *pki;
		GPtrArray *row;
		pki = LI_PKG_INFO (g_ptr_array_index (pkiarray, i));

		row = g_ptr_array_new_with_free_func (g_object_unref);
		g_ptr_array_add (row, g_object_ref (pki));
		g_ptr_array_add (priv->alist, row);
		g_hash_table_insert (priv->nindex,
					g_strdup (li_pkg_info_get_id (pki)),
					row);
	}

	/* connect the dots */
	for (i = 0; i < pkiarray->len; i++) {
		LiPkgInfo *pki;
		guint j;
		g_autoptr(GPtrArray) deps = NULL;
		pki = LI_PKG_INFO (g_ptr_array_index (pkiarray, i));

		deps = li_parse_dependencies_string (li_pkg_info_get_dependencies (pki));
		if (deps == NULL)
			continue;
		for (j = 0; j < deps->len; j++) {
			LiPkgInfo *ppki = NULL;
			LiPkgInfo *dep = LI_PKG_INFO (g_ptr_array_index (deps, j));


			ppki = li_find_satisfying_pkg (pkiarray, dep);
			if (ppki != NULL)
				li_package_graph_add_package (pg, pki, ppki, dep);
		}
	}

	return pg;
}

/**
 * li_package_graph_class_init:
 **/
static void
li_package_graph_class_init (LiPackageGraphClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_package_graph_finalize;

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
 * li_package_graph_new:
 *
 * Creates a new #LiPackageGraph.
 *
 * Returns: (transfer full): a #LiPackageGraph
 *
 **/
LiPackageGraph*
li_package_graph_new (void)
{
	LiPackageGraph *pg;
	pg = g_object_new (LI_TYPE_PACKAGE_GRAPH, NULL);
	return LI_PACKAGE_GRAPH (pg);
}
