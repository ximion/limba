/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2015 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:li-repository
 * @short_description: A local Limba package repository
 *
 */

#include "config.h"
#include "li-repository.h"

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <locale.h>
#include <gpgme.h>

#include "li-utils-private.h"
#include "li-pkg-index.h"

typedef struct _LiRepositoryPrivate	LiRepositoryPrivate;
struct _LiRepositoryPrivate
{
	LiPkgIndex *index;
	gchar *repo_path;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiRepository, li_repository, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_repository_get_instance_private (o))

/**
 * li_repository_finalize:
 **/
static void
li_repository_finalize (GObject *object)
{
	LiRepository *repo = LI_REPOSITORY (object);
	LiRepositoryPrivate *priv = GET_PRIVATE (repo);

	g_object_unref (priv->index);

	G_OBJECT_CLASS (li_repository_parent_class)->finalize (object);
}

/**
 * li_repository_init:
 **/
static void
li_repository_init (LiRepository *kr)
{
	LiRepositoryPrivate *priv = GET_PRIVATE (kr);

	priv->index = li_pkg_index_new ();
}

/**
 * li_repository_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_repository_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiRepositoryError");
	return quark;
}

/**
 * li_repository_class_init:
 **/
static void
li_repository_class_init (LiRepositoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_repository_finalize;
}

/**
 * li_repository_new:
 *
 * Creates a new #LiRepository.
 *
 * Returns: (transfer full): a #LiRepository
 *
 **/
LiRepository *
li_repository_new (void)
{
	LiRepository *repo;
	repo = g_object_new (LI_TYPE_REPOSITORY, NULL);
	return LI_REPOSITORY (repo);
}
