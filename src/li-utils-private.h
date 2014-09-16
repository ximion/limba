/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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

#ifndef __LI_UTILS_PRIVATE_H
#define __LI_UTILS_PRIVATE_H

#include <glib-object.h>

G_BEGIN_DECLS

gboolean		li_utils_touch_dir (const gchar* dirname);
gboolean		li_utils_delete_dir_recursive (const gchar* dirname);
gchar*			li_string_strip (const gchar* str);
GPtrArray*		li_utils_find_files_matching (const gchar* dir, const gchar* pattern, gboolean recursive);
GPtrArray*		li_utils_find_files (const gchar* dir, gboolean recursive);
gboolean		li_utils_is_root (void);
gchar*			li_str_replace (const gchar* str, const gchar* old_str, const gchar* new_str);

G_END_DECLS

#endif /* __LI_UTILS_PRIVATE_H */
