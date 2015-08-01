/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2014 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __LI_UTILS_H
#define __LI_UTILS_H

#include <glib-object.h>
#include "li-pkg-info.h"

G_BEGIN_DECLS

const gchar		*li_get_software_root (void);
void			li_set_verbose_mode (gboolean verbose);

gchar			*li_get_current_arch_h (void);
gchar			*li_get_arch_triplet (void);

gint			li_compare_versions (const gchar* a,
									const gchar *b);

LiPkgInfo		*li_parse_dependency_string (const gchar *depstr);
GPtrArray		*li_parse_dependencies_string (const gchar *depstr);

G_END_DECLS

#endif /* __LI_UTILS_H */
