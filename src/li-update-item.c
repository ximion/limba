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

/**
 * SECTION: li-update-item
 * @short_description: Describes a software update.
 */

#include "config.h"
#include "li-update-item.h"

#include "li-utils.h"
#include "li-utils-private.h"

typedef struct _LiUpdateItemPrivate	LiUpdateItemPrivate;
struct _LiUpdateItemPrivate
{
	LiPkgInfo *ipki;
	LiPkgInfo *apki;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiUpdateItem, li_update_item, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (li_update_item_get_instance_private (o))

/**
 * li_update_item_finalize:
 **/
static void
li_update_item_finalize (GObject *object)
{
	LiUpdateItem *uitem = LI_UPDATE_ITEM (object);
	LiUpdateItemPrivate *priv = GET_PRIVATE (uitem);

	g_object_unref (priv->ipki);
	g_object_unref (priv->apki);

	G_OBJECT_CLASS (li_update_item_parent_class)->finalize (object);
}

/**
 * li_update_item_init:
 **/
static void
li_update_item_init (LiUpdateItem *uitem)
{

}

/**
 * li_update_item_get_installed_pkg:
 *
 * Returns: (transfer none): A #LiPkgInfo of the installed software version.
 */
LiPkgInfo*
li_update_item_get_installed_pkg (LiUpdateItem *uitem)
{
	LiUpdateItemPrivate *priv = GET_PRIVATE (uitem);
	return priv->ipki;
}

/**
 * li_update_item_set_installed_pkg:
 */
void
li_update_item_set_installed_pkg (LiUpdateItem *uitem, LiPkgInfo *pki)
{
	LiUpdateItemPrivate *priv = GET_PRIVATE (uitem);
	priv->ipki = g_object_ref (pki);
}

/**
 * li_update_item_get_available_pkg:
 *
 * Returns: (transfer none): A #LiPkgInfo of the available new software version.
 */
LiPkgInfo*
li_update_item_get_available_pkg (LiUpdateItem *uitem)
{
	LiUpdateItemPrivate *priv = GET_PRIVATE (uitem);
	return priv->apki;
}

/**
 * li_update_item_set_available_pkg:
 */
void
li_update_item_set_available_pkg (LiUpdateItem *uitem, LiPkgInfo *pki)
{
	LiUpdateItemPrivate *priv = GET_PRIVATE (uitem);
	priv->apki = g_object_ref (pki);
}

/**
 * li_update_item_class_init:
 **/
static void
li_update_item_class_init (LiUpdateItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_update_item_finalize;
}

/**
 * li_update_item_new:
 *
 * Creates a new #LiUpdateItem.
 *
 * Returns: (transfer full): a #LiUpdateItem
 *
 **/
LiUpdateItem *
li_update_item_new (void)
{
	LiUpdateItem *uitem;
	uitem = g_object_new (LI_TYPE_UPDATE_ITEM, NULL);
	return LI_UPDATE_ITEM (uitem);
}

/**
 * li_update_item_new_with_packages:
 * @ipki: The installed package version
 * @apki: The available package version.
 *
 * Creates a new #LiUpdateItem with given #LiPkgInfo
 * references.
 *
 * Returns: (transfer full): a #LiUpdateItem
 *
 **/
LiUpdateItem *
li_update_item_new_with_packages (LiPkgInfo *ipki, LiPkgInfo *apki)
{
	LiUpdateItem *uitem;
	uitem = li_update_item_new ();

	li_update_item_set_installed_pkg (uitem, ipki);
	li_update_item_set_available_pkg (uitem, apki);

	return uitem;
}
