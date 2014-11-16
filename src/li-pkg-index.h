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

#ifndef __LI_PKG_INDEX_H
#define __LI_PKG_INDEX_H

#include <glib-object.h>
#include <gio/gio.h>
#include "li-pkg-info.h"

#define LI_TYPE_PKG_INDEX			(li_pkg_index_get_type())
#define LI_PKG_INDEX(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_PKG_INDEX, LiPkgIndex))
#define LI_PKG_INDEX_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_PKG_INDEX, LiPkgIndexClass))
#define LI_IS_PKG_INDEX(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_PKG_INDEX))
#define LI_IS_PKG_INDEX_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_PKG_INDEX))
#define LI_PKG_INDEX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_PKG_INDEX, LiPkgIndexClass))

G_BEGIN_DECLS

typedef struct _LiPkgIndex		LiPkgIndex;
typedef struct _LiPkgIndexClass	LiPkgIndexClass;

struct _LiPkgIndex
{
	GObject			parent;
};

struct _LiPkgIndexClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

GType			li_pkg_index_get_type	(void);
LiPkgIndex		*li_pkg_index_new		(void);

void			li_pkg_index_load_file (LiPkgIndex *pkidx,
											GFile *file);
void 			li_pkg_index_load_data (LiPkgIndex *pkidx,
										const gchar *data);
gboolean		li_pkg_index_save_to_file (LiPkgIndex *pkidx,
											const gchar *filename);

GPtrArray		*li_pkg_index_get_packages (LiPkgIndex *pkidx);
void			li_pkg_index_add_package (LiPkgIndex *pkidx,
											LiPkgInfo *pki);

gchar			*li_pkg_index_get_data (LiPkgIndex *pkidx);

G_END_DECLS

#endif /* __LI_PKG_INDEX_H */
