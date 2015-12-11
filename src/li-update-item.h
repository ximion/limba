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

G_BEGIN_DECLS

#define LI_TYPE_UPDATE_ITEM (li_update_item_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiUpdateItem, li_update_item, LI, UPDATE_ITEM, GObject)

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
