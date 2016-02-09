/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015-2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __LI_BUILD_CONF_H
#define __LI_BUILD_CONF_H

#include <glib-object.h>
#include <gio/gio.h>

#include "li-pkg-info.h"

#define LI_TYPE_BUILD_CONF		(li_build_conf_get_type())
#define LI_BUILD_CONF(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_BUILD_CONF, LiBuildConf))
#define LI_BUILD_CONF_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_BUILD_CONF, LiBuildConfClass))
#define LI_IS_BUILD_CONF(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_BUILD_CONF))
#define LI_IS_BUILD_CONF_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_BUILD_CONF))
#define LI_BUILD_CONF_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_BUILD_CONF, LiBuildConfClass))

G_BEGIN_DECLS

typedef struct _LiBuildConf		LiBuildConf;
typedef struct _LiBuildConfClass	LiBuildConfClass;

struct _LiBuildConf
{
	GObject			parent;
};

struct _LiBuildConfClass
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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (LiBuildConf, g_object_unref)

GType			li_build_conf_get_type	(void);
LiBuildConf		*li_build_conf_new	(void);

void			li_build_conf_open_file (LiBuildConf *bconf,
						GFile* file,
						GError **error);
void			li_build_conf_open_from_dir (LiBuildConf *bconf,
						const gchar *dir,
						GError **error);

GPtrArray		*li_build_conf_get_before_script (LiBuildConf *bconf);
GPtrArray		*li_build_conf_get_script (LiBuildConf *bconf);
GPtrArray		*li_build_conf_get_after_script (LiBuildConf *bconf);

LiPkgInfo		*li_build_conf_get_pkginfo (LiBuildConf *bconf);
gchar			*li_build_conf_get_extra_bundles_dir (LiBuildConf *bconf);

G_END_DECLS

#endif /* __LI_BUILD_CONF_H */
