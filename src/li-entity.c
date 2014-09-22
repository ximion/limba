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
 * SECTION:li-ipk-control
 * @short_description: Control metadata for IPK packages
 */

#include "config.h"
#include "li-entity.h"

typedef struct _LiEntityPrivate	LiEntityPrivate;
struct _LiEntityPrivate
{
	AsComponent *cpt;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiEntity, li_entity, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_entity_get_instance_private (o))

/**
 * li_entity_finalize:
 **/
static void
li_entity_finalize (GObject *object)
{
	LiEntity *entity = LI_ENTITY (object);
	LiEntityPrivate *priv = GET_PRIVATE (entity);

	g_object_unref (priv->cpt);

	G_OBJECT_CLASS (li_entity_parent_class)->finalize (object);
}

/**
 * li_entity_init:
 **/
static void
li_entity_init (LiEntity *entity)
{
	LiEntityPrivate *priv = GET_PRIVATE (entity);
	priv->cpt = as_component_new ();
}

/**
 * li_entity_class_init:
 **/
static void
li_entity_class_init (LiEntityClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_entity_finalize;
}

/**
 * li_entity_new:
 *
 * Creates a new #LiEntity.
 *
 * Returns: (transfer full): a #LiEntity
 *
 **/
LiEntity *
li_entity_new (void)
{
	LiEntity *entity;
	entity = g_object_new (LI_TYPE_ENTITY, NULL);
	return LI_ENTITY (entity);
}
