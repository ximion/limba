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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_PACKAGE_GRAPH_H
#define __LI_PACKAGE_GRAPH_H

#include <glib-object.h>

#include "li-pkg-info.h"
#include "li-package.h"

#define LI_TYPE_PACKAGE_GRAPH			(li_package_graph_get_type())
#define LI_PACKAGE_GRAPH(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_PACKAGE_GRAPH, LiPackageGraph))
#define LI_PACKAGE_GRAPH_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_PACKAGE_GRAPH, LiPackageGraphClass))
#define LI_IS_PACKAGE_GRAPH(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_PACKAGE_GRAPH))
#define LI_IS_PACKAGE_GRAPH_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_PACKAGE_GRAPH))
#define LI_PACKAGE_GRAPH_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_PACKAGE_GRAPH, LiPackageGraphClass))

G_BEGIN_DECLS

typedef struct _LiPackageGraph		LiPackageGraph;
typedef struct _LiPackageGraphClass	LiPackageGraphClass;

struct _LiPackageGraph
{
	GObject			parent;
};

struct _LiPackageGraphClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

GType				li_package_graph_get_type	(void);
LiPackageGraph		*li_package_graph_new		(void);

GNode				*li_package_graph_get_root (LiPackageGraph *pg);
void				li_package_graph_set_root (LiPackageGraph *pg,
											LiPkgInfo *data);
void				li_package_graph_set_root_install_todo (LiPackageGraph *pg,
															LiPackage *pkg);

void				li_package_graph_reset (LiPackageGraph *pg);

GPtrArray			*li_package_graph_branch_to_array (GNode *root);

GNode				*li_package_graph_add_package (LiPackageGraph *pg,
												GNode *parent,
												LiPkgInfo *pki);

GNode				*li_package_graph_add_package_install_todo (LiPackageGraph *pg,
															GNode *parent,
															LiPackage *pkg);

LiPackage			*li_package_graph_get_install_candidate (LiPackageGraph *pg,
															LiPkgInfo *pki);
gboolean			li_package_graph_mark_installed (LiPackageGraph *pg,
													LiPkgInfo *pki);

G_END_DECLS

#endif /* __LI_PACKAGE_GRAPH_H */
