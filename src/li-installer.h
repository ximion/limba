/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Matthias Klumpp <matthias@tenstral.net>
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

G_BEGIN_DECLS

#define LI_TYPE_INSTALLER (li_installer_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiInstaller, li_installer, LI, INSTALLER, GObject)

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

/**
 * LiInstallerError:
 * @LI_INSTALLER_ERROR_FAILED:			Generic failure
 * @LI_INSTALLER_ERROR_INTERNAL:		Internal error
 * @LI_INSTALLER_ERROR_DEPENDENCY_NOT_FOUND:	A dependency was not found
 * @LI_INSTALLER_ERROR_FOUNDATION_NOT_FOUND:	A system dependency was not found
 *
 * The error type.
 **/
typedef enum {
	LI_INSTALLER_ERROR_FAILED,
	LI_INSTALLER_ERROR_INTERNAL,
	LI_INSTALLER_ERROR_DEPENDENCY_NOT_FOUND,
	LI_INSTALLER_ERROR_FOUNDATION_NOT_FOUND,
	/*< private >*/
	LI_INSTALLER_ERROR_LAST
} LiInstallerError;

#define	LI_INSTALLER_ERROR li_installer_error_quark ()
GQuark			li_installer_error_quark (void);

LiInstaller		*li_installer_new (void);

gboolean		li_installer_open_file (LiInstaller *inst,
							const gchar *filename,
							GError **error);
gboolean		li_installer_open_remote (LiInstaller *inst,
							const gchar *pkgid,
							GError **error);

gboolean		li_installer_install (LiInstaller *inst,
						GError **error);

LiPkgInfo		*li_installer_get_package_info (LiInstaller *inst);
gchar			*li_installer_get_appstream_data (LiInstaller *inst);
LiTrustLevel		li_installer_get_package_trust_level (LiInstaller *inst,
								GError **error);

void			li_installer_set_ignore_foundations  (LiInstaller *inst,
								gboolean ignore);
void			li_installer_set_allow_insecure (LiInstaller *inst,
							 gboolean insecure);

G_END_DECLS

#endif /* __LI_INSTALLER_H */
