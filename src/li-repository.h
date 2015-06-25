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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_REPOSITORY_H
#define __LI_REPOSITORY_H

#include <glib-object.h>
#include "li-package.h"

#define LI_TYPE_REPOSITORY			(li_repository_get_type())
#define LI_REPOSITORY(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_REPOSITORY, LiRepository))
#define LI_REPOSITORY_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_REPOSITORY, LiRepositoryClass))
#define LI_IS_REPOSITORY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_REPOSITORY))
#define LI_IS_REPOSITORY_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_REPOSITORY))
#define LI_REPOSITORY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_REPOSITORY, LiRepositoryClass))

G_BEGIN_DECLS

/**
 * LiRepositoryError:
 * @LI_REPOSITORY_ERROR_FAILED:			Generic failure
 * @LI_REPOSITORY_ERROR_NO_REPO:		The directory is no Limba repository
 * @LI_REPOSITORY_ERROR_PKG_EXISTS:		Package was already in the repository and can not be added again.
 * @LI_REPOSITORY_ERROR_EMBEDDED_COPY:	The package contains embedded other packages, which is not allowed.
 * @LI_REPOSITORY_ERROR_SIGN:			Signing of repository failed.
 *
 * The error type.
 **/
typedef enum {
	LI_REPOSITORY_ERROR_FAILED,
	LI_REPOSITORY_ERROR_NO_REPO,
	LI_REPOSITORY_ERROR_PKG_EXISTS,
	LI_REPOSITORY_ERROR_EMBEDDED_COPY,
	LI_REPOSITORY_ERROR_SIGN,
	/*< private >*/
	LI_REPOSITORY_ERROR_LAST
} LiRepositoryError;

#define	LI_REPOSITORY_ERROR li_repository_error_quark ()
GQuark li_repository_error_quark (void);

typedef struct _LiRepository		LiRepository;
typedef struct _LiRepositoryClass	LiRepositoryClass;

struct _LiRepository
{
	GObject			parent;
};

struct _LiRepositoryClass
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

GType			li_repository_get_type	(void);
LiRepository	*li_repository_new		(void);

gboolean		li_repository_open (LiRepository *repo,
									const gchar *directory,
									GError **error);

gboolean		li_repository_save (LiRepository *repo,
									GError **error);

gboolean		li_repository_add_package (LiRepository *repo,
									const gchar *pkg_fname,
									GError **error);

gboolean		li_repository_create_icon_tarballs (LiRepository *repo,
									GError **error);

G_END_DECLS

#endif /* __LI_REPOSITORY_H */
