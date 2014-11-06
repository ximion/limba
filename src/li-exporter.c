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
 * SECTION:li-exporter
 * @short_description: Export certain files from the software environment in /opt to achieve better system integration.
 */

#include "config.h"
#include "li-exporter.h"

typedef struct _LiExporterPrivate	LiExporterPrivate;
struct _LiExporterPrivate
{
	guint dummy;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiExporter, li_exporter, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_exporter_get_instance_private (o))

/**
 * li_exporter_finalize:
 **/
static void
li_exporter_finalize (GObject *object)
{
#if 0
	LiExporter *exp = LI_EXPORTER (object);
	LiExporterPrivate *priv = GET_PRIVATE (exp);
#endif

	G_OBJECT_CLASS (li_exporter_parent_class)->finalize (object);
}

/**
 * li_exporter_init:
 **/
static void
li_exporter_init (LiExporter *exp)
{

}

/**
 * li_exporter_class_init:
 **/
static void
li_exporter_class_init (LiExporterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_exporter_finalize;
}

/**
 * li_exporter_new:
 *
 * Creates a new #LiExporter.
 *
 * Returns: (transfer full): a #LiExporter
 *
 **/
LiExporter *
li_exporter_new (void)
{
	LiExporter *exp;
	exp = g_object_new (LI_TYPE_EXPORTER, NULL);
	return LI_EXPORTER (exp);
}
