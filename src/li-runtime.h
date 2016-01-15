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

#ifndef __LI_RUNTIME_H
#define __LI_RUNTIME_H

#include <glib-object.h>
#include "li-pkg-info.h"

G_BEGIN_DECLS

#define LI_TYPE_RUNTIME (li_runtime_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiRuntime, li_runtime, LI, RUNTIME, GObject)

struct _LiRuntimeClass
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

LiRuntime		*li_runtime_new (void);

gboolean		li_runtime_load_from_file (LiRuntime *rt,
							const gchar *fname,
							GError **error);
gboolean		li_runtime_load_by_uuid (LiRuntime *rt,
							const gchar *uuid,
							GError **error);

gboolean		li_runtime_save (LiRuntime *rt,
					 GError **error);

const gchar		*li_runtime_get_uuid (LiRuntime *rt);

LiRuntime		*li_runtime_create_with_members (GPtrArray *members,
							 GError **error);

GHashTable		*li_runtime_get_requirements (LiRuntime *rt);
GHashTable		*li_runtime_get_members (LiRuntime *rt);

void			li_runtime_add_package (LiRuntime *rt,
						LiPkgInfo *pki);
void			li_runtime_remove_package (LiRuntime *rt,
							LiPkgInfo *pki);

gboolean		li_runtime_remove (LiRuntime *rt);

G_END_DECLS

#endif /* __LI_RUNTIME_H */
