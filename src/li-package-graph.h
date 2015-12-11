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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_PACKAGE_GRAPH_H
#define __LI_PACKAGE_GRAPH_H

#include <glib-object.h>

#include "li-pkg-info.h"
#include "li-package.h"

G_BEGIN_DECLS

#define LI_TYPE_PACKAGE_GRAPH (li_package_graph_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiPackageGraph, li_package_graph, LI, PACKAGE_GRAPH, GObject)

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

LiPackageGraph		*li_package_graph_new (void);

void			li_package_graph_initialize (LiPackageGraph *pg,
							GError **error);

GNode			*li_package_graph_get_root (LiPackageGraph *pg);
void			li_package_graph_set_root (LiPackageGraph *pg,
							LiPkgInfo *data);
void			li_package_graph_set_root_install_todo (LiPackageGraph *pg,
									LiPackage *pkg);

void			li_package_graph_reset (LiPackageGraph *pg);

GPtrArray		*li_package_graph_branch_to_array (GNode *root);

GNode			*li_package_graph_add_package (LiPackageGraph *pg,
							GNode *parent,
							LiPkgInfo *pki,
							LiPkgInfo *satisfied_dep);

GNode			*li_package_graph_add_package_install_todo (LiPackageGraph *pg,
									GNode *parent,
									LiPackage *pkg,
									LiPkgInfo *satisfied_dep);

LiPackage		*li_package_graph_get_install_candidate (LiPackageGraph *pg,
									LiPkgInfo *pki);
gboolean		li_package_graph_mark_installed (LiPackageGraph *pg,
								LiPkgInfo *pki);

guint			li_package_graph_get_install_todo_count (LiPackageGraph *pg);

gboolean		li_package_graph_test_foundation_dependency (LiPackageGraph *pg,
									LiPkgInfo *dep_pki,
									GError **error);

LiPkgInfo		*li_find_satisfying_pkg (GList *pkglist,
						 LiPkgInfo *dep);

G_END_DECLS

#endif /* __LI_PACKAGE_GRAPH_H */
