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
 * SECTION:li-polylinker
 * @short_description: Link software components together to form a framework (environment) to run applications in.
 *
 * NOTE: The name of this class is just a placeholder, until it's clearly defined what a "framework" and
 * "environment" actually means this context.
 */

#include "config.h"
#include "li-polylinker.h"

#include <glib/gi18n-lib.h>

#include "li-pkg-info.h"
#include "li-manager.h"
#include "li-utils-private.h"

typedef struct _LiPolylinkerPrivate	LiPolylinkerPrivate;
struct _LiPolylinkerPrivate
{
	guint dummy;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPolylinker, li_polylinker, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_polylinker_get_instance_private (o))

/**
 * li_polylinker_finalize:
 **/
static void
li_polylinker_finalize (GObject *object)
{
#if 0
	LiPolylinker *plink = LI_POLYLINKER (object);
	LiPolylinkerPrivate *priv = GET_PRIVATE (plink);
#endif

	G_OBJECT_CLASS (li_polylinker_parent_class)->finalize (object);
}

/**
 * li_polylinker_init:
 **/
static void
li_polylinker_init (LiPolylinker *plink)
{
}

/**
 * li_polylinker_get_framework_for:
 * @sw: A list of software as #LiPkgInfo
 *
 * Get the ID of a framework which provides the software mentioned
 * in the sw array.
 * In case it doesn't exist yet, generate it.
 */
gchar*
li_polylinker_get_framework_for (LiPolylinker *plink, GPtrArray *sw)
{
	//LiPolylinkerPrivate *priv = GET_PRIVATE (plink);

	return NULL;
}

/**
 * li_polylinker_class_init:
 **/
static void
li_polylinker_class_init (LiPolylinkerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_polylinker_finalize;
}

/**
 * li_polylinker_new:
 *
 * Creates a new #LiPolylinker.
 *
 * Returns: (transfer full): a #LiPolylinker
 *
 **/
LiPolylinker *
li_polylinker_new (void)
{
	LiPolylinker *plink;
	plink = g_object_new (LI_TYPE_POLYLINKER, NULL);
	return LI_POLYLINKER (plink);
}
