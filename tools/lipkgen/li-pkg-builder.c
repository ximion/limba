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
 * SECTION:li-pkg-builder
 * @short_description: Creates Limba packages
 */

#include "config.h"
#include "li-pkg-builder.h"

#include <limba.h>
#include <archive_entry.h>
#include <archive.h>

typedef struct _LiPkgBuilderPrivate	LiPkgBuilderPrivate;
struct _LiPkgBuilderPrivate
{
	gchar *dir;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPkgBuilder, li_pkg_builder, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_pkg_builder_get_instance_private (o))

/**
 * li_pkg_builder_finalize:
 **/
static void
li_pkg_builder_finalize (GObject *object)
{
	LiPkgBuilder *builder = LI_PKG_BUILDER (object);
	LiPkgBuilderPrivate *priv = GET_PRIVATE (builder);

	g_free (priv->dir);

	G_OBJECT_CLASS (li_pkg_builder_parent_class)->finalize (object);
}

/**
 * li_pkg_builder_init:
 **/
static void
li_pkg_builder_init (LiPkgBuilder *builder)
{
}

/**
 * li_pkg_builder_create_package_from_dir:
 */
gboolean
li_pkg_builder_create_package_from_dir (LiPkgBuilder *builder, const gchar *metadata_dir, GError **error)
{
	return FALSE;
}

/**
 * li_builder_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_builder_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiBuilderError");
	return quark;
}

/**
 * li_pkg_builder_class_init:
 **/
static void
li_pkg_builder_class_init (LiPkgBuilderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_pkg_builder_finalize;
}

/**
 * li_pkg_builder_new:
 *
 * Creates a new #LiPkgBuilder.
 *
 * Returns: (transfer full): a #LiPkgBuilder
 *
 **/
LiPkgBuilder *
li_pkg_builder_new (void)
{
	LiPkgBuilder *builder;
	builder = g_object_new (LI_TYPE_PKG_BUILDER, NULL);
	return LI_PKG_BUILDER (builder);
}
