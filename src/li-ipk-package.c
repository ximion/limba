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
 * SECTION:li-ipk-package
 * @short_description: Representation of a complete Listaller package
 */

#include "config.h"
#include "li-ipk-package.h"

typedef struct _LiIPKPackagePrivate	LiIPKPackagePrivate;
struct _LiIPKPackagePrivate
{

};

G_DEFINE_TYPE_WITH_PRIVATE (LiIPKPackage, li_ipk_package, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_ipk_package_get_instance_private (o))

/**
 * li_ipk_package_finalize:
 **/
static void
li_ipk_package_finalize (GObject *object)
{
	/* LiIPKPackage *ipk = LI_IPK_PACKAGE (object);
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk); */

	G_OBJECT_CLASS (li_ipk_package_parent_class)->finalize (object);
}

/**
 * li_ipk_package_init:
 **/
static void
li_ipk_package_init (LiIPKPackage *ipk)
{

}

/**
 * li_ipk_package_class_init:
 **/
static void
li_ipk_package_class_init (LiIPKPackageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_ipk_package_finalize;
}

/**
 * li_ipk_package_new:
 *
 * Creates a new #LiIPKPackage.
 *
 * Returns: (transfer full): a #LiIPKPackage
 *
 **/
LiIPKPackage *
li_ipk_package_new (void)
{
	LiIPKPackage *ipk;
	ipk = g_object_new (LI_TYPE_IPK_PACKAGE, NULL);
	return LI_IPK_PACKAGE (ipk);
}
