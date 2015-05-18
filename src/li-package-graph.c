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
 * SECTION:li-package-graph
 * @short_description: Represents a package dependency graph
 *
 * This class represents a graph of packages (nodes are #LiPkgInfo instances, edges represent a dependency),
 * as well as a hash-table of packages which need installation.
 * It is mainly used by #LiInstaller at time.
 */

#include "config.h"
#include "li-package-graph.h"

#include <math.h>

typedef struct _LiPackageGraphPrivate	LiPackageGraphPrivate;
struct _LiPackageGraphPrivate
{
	GNode *root_pkg;
	GHashTable *install_todo;

	guint progress;
	guint max_progress;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPackageGraph, li_package_graph, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_package_graph_get_instance_private (o))

enum {
	SIGNAL_STAGE_CHANGED,
	SIGNAL_PROGRESS,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

static void li_package_graph_teardown (GNode *root);

/**
 * li_package_graph_finalize:
 **/
static void
li_package_graph_finalize (GObject *object)
{
	LiPackageGraph *pg = LI_PACKAGE_GRAPH (object);
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	li_package_graph_teardown (priv->root_pkg);
	g_hash_table_unref (priv->install_todo);

	G_OBJECT_CLASS (li_package_graph_parent_class)->finalize (object);
}

/**
 * li_package_graph_init:
 **/
static void
li_package_graph_init (LiPackageGraph *pg)
{
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	priv->root_pkg = g_node_new (NULL);
	priv->install_todo = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
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
 * _li_package_graph_free_node:
 */
static gboolean
_li_package_graph_free_node (GNode *node, gpointer data)
{
	if (node->data != NULL)
		g_object_unref (node->data);

	return FALSE;
}

/**
 * li_package_graph_teardown:
 */
static void
li_package_graph_teardown (GNode *root)
{
	g_node_traverse (root,
					G_IN_ORDER,
					G_TRAVERSE_ALL,
					-1,
					_li_package_graph_free_node,
					NULL);
	g_node_destroy (root);
}

/**
 * li_package_graph_add_package:
 * @pki: The information about an installed package
 * @satisfied_dep: The dependency this package satisfies, or %NULL
 *
 * Returns: A reference to the new node
 */
GNode*
li_package_graph_add_package (LiPackageGraph *pg, GNode *parent, LiPkgInfo *pki, LiPkgInfo *satisfied_dep)
{
	GNode *node;

	node = g_node_new (g_object_ref (pki));
	g_node_append (parent, node);
	g_debug ("Component %s-%s found.", li_pkg_info_get_name (pki), li_pkg_info_get_version (pki));

	if (satisfied_dep != NULL)
		li_pkg_info_set_version_relation (pki,
									li_pkg_info_get_version_relation (satisfied_dep));

	return node;
}

/**
 * li_package_graph_add_package_install_todo:
 * @pkg: The package required to be installed
 * @satisfied_dep: The dependency this package satisfies, or %NULL
 *
 * Returns: A reference to the new node
 */
GNode*
li_package_graph_add_package_install_todo (LiPackageGraph *pg, GNode *parent, LiPackage *pkg, LiPkgInfo *satisfied_dep)
{
	GNode *node;
	gboolean ret;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	node = g_node_new (g_object_ref (li_package_get_info (pkg)));
	g_node_append (parent, node);

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

	if (satisfied_dep != NULL)
		li_pkg_info_set_version_relation (li_package_get_info (pkg),
									li_pkg_info_get_version_relation (satisfied_dep));

	priv->max_progress = g_hash_table_size (priv->install_todo)*100;

	return node;
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
 * _li_package_graph_add_pki_to_array:
 */
static gboolean
_li_package_graph_add_pki_to_array (GNode *node, gpointer data)
{
	GPtrArray *array = (GPtrArray*) data;
	g_ptr_array_add (array, node->data);

	return FALSE;
}

/**
 * li_package_graph_branch_to_array:
 *
 * Get an array of #LiPkgInfo objects this node depends on.
 */
GPtrArray*
li_package_graph_branch_to_array (GNode *root)
{
	GPtrArray *array;

	if (root->children == NULL)
		return NULL;

	array = g_ptr_array_new ();
	g_node_traverse (root,
					G_PRE_ORDER,
					G_TRAVERSE_ALL,
					-1,
					_li_package_graph_add_pki_to_array,
					array);

	/* remove the root node value from the array */
	if (array->len > 0)
		g_ptr_array_remove_index (array, 0);

	/* return null in case the array is empty */
	if (array->len == 0) {
		g_ptr_array_unref (array);
		return NULL;
	}

	return array;
}

/**
 * li_package_graph_reset:
 *
 * Remove all nodes from the tree, except for the root node.
 */
void
li_package_graph_reset (LiPackageGraph *pg)
{
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	GNode *child = priv->root_pkg->children;
	if (child == NULL)
		return;

	while (child->next != NULL)
		li_package_graph_teardown (child->next);
}

/**
 * li_package_graph_set_root:
 */
void
li_package_graph_set_root (LiPackageGraph *pg, LiPkgInfo *data)
{
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);
	li_package_graph_teardown (priv->root_pkg);
	priv->root_pkg = g_node_new (g_object_ref (data));
}

/**
 * li_package_graph_set_root_install_todo:
 */
void
li_package_graph_set_root_install_todo (LiPackageGraph *pg, LiPackage *pkg)
{
	gboolean ret;
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);

	li_package_graph_set_root (pg, li_package_get_info (pkg));
	ret = g_hash_table_insert (priv->install_todo,
						g_strdup (li_package_get_id (pkg)),
						g_object_ref (pkg));

	if (ret) {
		/* connect signals */
		g_signal_connect (pkg, "progress",
						G_CALLBACK (li_package_graph_package_progress_cb), pg);
		g_signal_connect (pkg, "stage-changed",
						G_CALLBACK (li_package_graph_package_stage_changed_cb), pg);
	}

	priv->max_progress = g_hash_table_size (priv->install_todo)*100;
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
 * li_package_graph_get_root:
 */
GNode*
li_package_graph_get_root (LiPackageGraph *pg)
{
	LiPackageGraphPrivate *priv = GET_PRIVATE (pg);
	return priv->root_pkg;
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
