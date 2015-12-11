/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_PKGBUILDER_H
#define __LI_PKGBUILDER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define LI_TYPE_PKG_BUILDER (li_pkg_builder_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiPkgBuilder, li_pkg_builder, LI, PKG_BUILDER, GObject)

struct _LiPkgBuilderClass
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

/**
 * LiBuilderError:
 * @LI_BUILDER_ERROR_FAILED:		Generic failure
 * @LI_BUILDER_ERROR_NOT_FOUND:		A required file or entity was not found
 * @LI_BUILDER_ERROR_WRITE:		An error occured while writing data
 * @LI_BUILDER_ERROR_SIGN:		Error while signing the package
 *
 * The error type.
 **/
typedef enum {
	LI_BUILDER_ERROR_FAILED,
	LI_BUILDER_ERROR_NOT_FOUND,
	LI_BUILDER_ERROR_WRITE,
	LI_BUILDER_ERROR_SIGN,
	/*< private >*/
	LI_BUILDER_ERROR_LAST
} LiBuilderError;

#define	LI_BUILDER_ERROR li_builder_error_quark ()
GQuark li_builder_error_quark (void);

LiPkgBuilder		*li_pkg_builder_new (void);

gboolean		li_pkg_builder_create_package_from_dir (LiPkgBuilder *builder,
								const gchar *data_dir,
								const gchar *out_fname,
								GError **error);

gboolean		li_pkg_builder_get_sign_package (LiPkgBuilder *builder);
void			li_pkg_builder_set_sign_package (LiPkgBuilder *builder,
							gboolean sign);

G_END_DECLS

#endif /* __LI_PKGBUILDER_H */
