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

#ifndef __LI_INSTALLER_H
#define __LI_INSTALLER_H

#include <glib-object.h>
#include "li-package.h"

#define LI_TYPE_INSTALLER			(li_installer_get_type())
#define LI_INSTALLER(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_INSTALLER, LiInstaller))
#define LI_INSTALLER_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_INSTALLER, LiInstallerClass))
#define LI_IS_INSTALLER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_INSTALLER))
#define LI_IS_INSTALLER_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_INSTALLER))
#define LI_INSTALLER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_INSTALLER, LiInstallerClass))

G_BEGIN_DECLS

/**
 * LiInstallerError:
 * @LI_INSTALLER_ERROR_FAILED:					Generic failure
 * @LI_INSTALLER_ERROR_DEPENDENCY_NOT_FOUND:	A dependency was not found
 *
 * The error type.
 **/
typedef enum {
	LI_INSTALLER_ERROR_FAILED,
	LI_INSTALLER_ERROR_DEPENDENCY_NOT_FOUND,
	/*< private >*/
	LI_INSTALLER_ERROR_LAST
} LiInstallerError;

#define	LI_INSTALLER_ERROR li_installer_error_quark ()
GQuark li_installer_error_quark (void);

typedef struct _LiInstaller		LiInstaller;
typedef struct _LiInstallerClass	LiInstallerClass;

struct _LiInstaller
{
	GObject			parent;
};

struct _LiInstallerClass
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

GType			li_installer_get_type	(void);
LiInstaller		*li_installer_new		(void);

gboolean		li_installer_open_file (LiInstaller *inst,
									const gchar *filename,
									GError **error);
gboolean		li_installer_install (LiInstaller *inst,
										GError **error);

LiPkgInfo		*li_installer_get_package_info (LiInstaller *inst);

G_END_DECLS

#endif /* __LI_INSTALLER_H */
