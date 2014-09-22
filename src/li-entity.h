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

#if !defined (__LISTALLER_H) && !defined (LI_COMPILATION)
#error "Only <listaller.h> can be included directly."
#endif

#ifndef __LI_ENTITY_H
#define __LI_ENTITY_H

#include <glib-object.h>
#include <appstream.h>

#define LI_TYPE_ENTITY			(li_entity_get_type())
#define LI_ENTITY(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_ENTITY, LiEntity))
#define LI_ENTITY_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_ENTITY, LiEntityClass))
#define LI_IS_ENTITY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_ENTITY))
#define LI_IS_ENTITY_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_ENTITY))
#define LI_ENTITY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_ENTITY, LiEntityClass))

G_BEGIN_DECLS

typedef struct _LiEntity		LiEntity;
typedef struct _LiEntityClass	LiEntityClass;

struct _LiEntity
{
	GObject			parent;
};

struct _LiEntityClass
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

GType			li_entity_get_type	(void);
LiEntity		*li_entity_new		(void);

G_END_DECLS

#endif /* __LI_ENTITY_H */
