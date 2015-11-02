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

#ifndef __LI_UPDATE_ITEM_H
#define __LI_UPDATE_ITEM_H

#include <glib-object.h>
#include <gio/gio.h>
#include "li-pkg-info.h"

#define LI_TYPE_UPDATE_ITEM		(li_update_item_get_type())
#define LI_UPDATE_ITEM(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_UPDATE_ITEM, LiUpdateItem))
#define LI_UPDATE_ITEM_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_UPDATE_ITEM, LiUpdateItemClass))
#define LI_IS_UPDATE_ITEM(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_UPDATE_ITEM))
#define LI_IS_UPDATE_ITEM_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_UPDATE_ITEM))
#define LI_UPDATE_ITEM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_UPDATE_ITEM, LiUpdateItemClass))

G_BEGIN_DECLS

typedef struct _LiUpdateItem		LiUpdateItem;
typedef struct _LiUpdateItemClass	LiUpdateItemClass;

struct _LiUpdateItem
{
	GObject			parent;
};

struct _LiUpdateItemClass
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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (LiUpdateItem, g_object_unref)

GType			li_update_item_get_type (void);
LiUpdateItem		*li_update_item_new (void);
LiUpdateItem		*li_update_item_new_with_packages (LiPkgInfo *ipki,
								LiPkgInfo *apki);

LiPkgInfo		*li_update_item_get_available_pkg (LiUpdateItem *uitem);
void			li_update_item_set_available_pkg (LiUpdateItem *uitem,
								LiPkgInfo *pki);

LiPkgInfo		*li_update_item_get_installed_pkg (LiUpdateItem *uitem);
void			li_update_item_set_installed_pkg (LiUpdateItem *uitem,
								LiPkgInfo *pki);

G_END_DECLS

#endif /* __LI_UPDATE_ITEM_H */
