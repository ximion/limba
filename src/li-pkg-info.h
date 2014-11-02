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

#ifndef __LI_PKG_INFO_H
#define __LI_PKG_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

#define LI_TYPE_PKG_INFO			(li_pkg_info_get_type())
#define LI_PKG_INFO(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_PKG_INFO, LiPkgInfo))
#define LI_PKG_INFO_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_PKG_INFO, LiPkgInfoClass))
#define LI_IS_PKG_INFO(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_PKG_INFO))
#define LI_IS_PKG_INFO_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_PKG_INFO))
#define LI_PKG_INFO_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_PKG_INFO, LiPkgInfoClass))

G_BEGIN_DECLS

typedef struct _LiPkgInfo		LiPkgInfo;
typedef struct _LiPkgInfoClass	LiPkgInfoClass;

struct _LiPkgInfo
{
	GObject			parent;
};

struct _LiPkgInfoClass
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

GType			li_pkg_info_get_type	(void);
LiPkgInfo	*li_pkg_info_new		(void);

void			li_pkg_info_load_file (LiPkgInfo *pki,
											GFile *file);
void 			li_pkg_info_load_data (LiPkgInfo *pki,
										const gchar *data);
gboolean		li_pkg_info_save_to_file (LiPkgInfo *pki,
											const gchar *filename);

const gchar		*li_pkg_info_get_version (LiPkgInfo *pki);
void			li_pkg_info_set_version (LiPkgInfo *pki,
										const gchar *version);

const gchar		*li_pkg_info_get_name (LiPkgInfo *pki);
void			li_pkg_info_set_name (LiPkgInfo *pki,
										const gchar *name);

const gchar		*li_pkg_info_get_framework_dependency (LiPkgInfo *pki);
void			li_pkg_info_set_framework_dependency (LiPkgInfo *pki,
										const gchar *uuid);

const gchar		*li_pkg_info_get_dependencies (LiPkgInfo *pki);
void			li_pkg_info_set_dependencies (LiPkgInfo *pki,
										const gchar *deps_string);

const gchar		*li_pkg_info_get_id (LiPkgInfo *pki);


G_END_DECLS

#endif /* __LI_PKG_INFO_H */
