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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_IPK_PACKAGE_H
#define __LI_IPK_PACKAGE_H

#include <glib-object.h>

#define LI_TYPE_IPK_PACKAGE			(li_ipk_package_get_type())
#define LI_IPK_PACKAGE(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_IPK_PACKAGE, LiIPKPackage))
#define LI_IPK_PACKAGE_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_IPK_PACKAGE, LiIPKPackageClass))
#define LI_IS_IPK_PACKAGE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_IPK_PACKAGE))
#define LI_IS_IPK_PACKAGE_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_IPK_PACKAGE))
#define LI_IPK_PACKAGE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_IPK_PACKAGE, LiIPKPackageClass))

G_BEGIN_DECLS

/**
 * LiPackageError:
 * @LI_PACKAGE_ERROR_FAILED:		Generic failure
 * @LI_PACKAGE_ERROR_NOT_FOUND:		A required file or entity was not found
 * @LI_PACKAGE_ERROR_ARCHIVE:		Error in the archive structure
 * @LI_PACKAGE_ERROR_DATA_MISSING:	Some data is missing in the archive
 * @LI_PACKAGE_ERROR_OVERRIDE:		Could not override file
 * @LI_PACKAGE_ERROR_EXTRACT:		Could not extract data
 *
 * The error type.
 **/
typedef enum {
	LI_PACKAGE_ERROR_FAILED,
	LI_PACKAGE_ERROR_NOT_FOUND,
	LI_PACKAGE_ERROR_ARCHIVE,
	LI_PACKAGE_ERROR_DATA_MISSING,
	LI_PACKAGE_ERROR_OVERRIDE,
	LI_PACKAGE_ERROR_EXTRACT,
	/*< private >*/
	LI_PACKAGE_ERROR_LAST
} LiPackageError;

#define	LI_PACKAGE_ERROR li_package_error_quark ()
GQuark li_package_error_quark (void);

typedef struct _LiIPKPackage		LiIPKPackage;
typedef struct _LiIPKPackageClass	LiIPKPackageClass;

struct _LiIPKPackage
{
	GObject			parent;
};

struct _LiIPKPackageClass
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

GType			li_ipk_package_get_type	(void);
LiIPKPackage	*li_ipk_package_new		(void);

gboolean		li_ipk_package_open_file (LiIPKPackage *ipk,
										const gchar *filename,
										GError **error);
gboolean		li_ipk_package_install (LiIPKPackage *ipk,
										GError **error);

const gchar		*li_ipk_package_get_install_root (LiIPKPackage *ipk);
void			li_ipk_package_set_install_root (LiIPKPackage *ipk,
												const gchar *dir);

const gchar		*li_ipk_package_get_id (LiIPKPackage *ipk);
void			li_ipk_package_set_id (LiIPKPackage *ipk,
										const gchar *unique_name);

G_END_DECLS

#endif /* __LI_IPK_PACKAGE_H */
