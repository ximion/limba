/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C)      2012 Colin Walters <walters@verbum.org>
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
#include <appstream.h>

G_BEGIN_DECLS

#define GS_DEFINE_CLEANUP_FUNCTION(Type, name, func) \
  static inline void name (void *v) \
  { \
    func (*(Type*)v); \
  }

#define GS_DEFINE_CLEANUP_FUNCTION0(Type, name, func) \
  static inline void name (void *v) \
  { \
    if (*(Type*)v) \
      func (*(Type*)v); \
  }

#define GS_DEFINE_CLEANUP_FUNCTIONt(Type, name, func) \
  static inline void name (void *v) \
  { \
    if (*(Type*)v) \
      func (*(Type*)v, TRUE); \
  }

GS_DEFINE_CLEANUP_FUNCTION0(GArray*, gs_local_array_unref, g_array_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GBytes*, gs_local_bytes_unref, g_bytes_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GRegex*, gs_local_regex_unref, g_regex_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GChecksum*, gs_local_checksum_free, g_checksum_free)
GS_DEFINE_CLEANUP_FUNCTION0(GDir*, gs_local_dir_close, g_dir_close)
GS_DEFINE_CLEANUP_FUNCTION0(GError*, gs_local_free_error, g_error_free)
GS_DEFINE_CLEANUP_FUNCTION0(GHashTable*, gs_local_hashtable_unref, g_hash_table_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GKeyFile*, gs_local_keyfile_unref, g_key_file_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GMarkupParseContext*, gs_local_markup_parse_context_unref, g_markup_parse_context_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GObject*, gs_local_obj_unref, g_object_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GPtrArray*, gs_local_ptrarray_unref, g_ptr_array_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GTimer*, gs_local_destroy_timer, g_timer_destroy)
GS_DEFINE_CLEANUP_FUNCTION0(GVariantBuilder*, gs_local_variant_builder_unref, g_variant_builder_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GVariant*, gs_local_variant_unref, g_variant_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GVariantIter*, gs_local_variant_iter_free, g_variant_iter_free)

GS_DEFINE_CLEANUP_FUNCTIONt(GString*, gs_local_free_string, g_string_free)

GS_DEFINE_CLEANUP_FUNCTION(char**, gs_local_strfreev, g_strfreev)
GS_DEFINE_CLEANUP_FUNCTION(GList*, gs_local_free_list, g_list_free)
GS_DEFINE_CLEANUP_FUNCTION(void*, gs_local_free, g_free)

#define _cleanup_dir_close_ __attribute__ ((cleanup(gs_local_dir_close)))
#define _cleanup_timer_destroy_ __attribute__ ((cleanup(gs_local_destroy_timer)))
#define _cleanup_free_ __attribute__ ((cleanup(gs_local_free)))
#define _cleanup_checksum_free_ __attribute__ ((cleanup(gs_local_checksum_free)))
#define _cleanup_error_free_ __attribute__ ((cleanup(gs_local_free_error)))
#define _cleanup_list_free_ __attribute__ ((cleanup(gs_local_free_list)))
#define _cleanup_string_free_ __attribute__ ((cleanup(gs_local_free_string)))
#define _cleanup_strv_free_ __attribute__ ((cleanup(gs_local_strfreev)))
#define _cleanup_variant_iter_free_ __attribute__ ((cleanup(gs_local_variant_iter_free)))
#define _cleanup_array_unref_ __attribute__ ((cleanup(gs_local_array_unref)))
#define _cleanup_bytes_unref_ __attribute__ ((cleanup(gs_local_bytes_unref)))
#define _cleanup_regex_unref_ __attribute__ ((cleanup(gs_local_regex_unref)))
#define _cleanup_hashtable_unref_ __attribute__ ((cleanup(gs_local_hashtable_unref)))
#define _cleanup_keyfile_unref_ __attribute__ ((cleanup(gs_local_keyfile_unref)))
#define _cleanup_markup_parse_context_unref_ __attribute__ ((cleanup(gs_local_markup_parse_context_unref)))
#define _cleanup_object_unref_ __attribute__ ((cleanup(gs_local_obj_unref)))
#define _cleanup_ptrarray_unref_ __attribute__ ((cleanup(gs_local_ptrarray_unref)))
#define _cleanup_variant_unref_ __attribute__ ((cleanup(gs_local_variant_unref)))

#define	LI_PREFIXDIR li_get_prefixdir ()

gboolean		li_str_empty (const gchar* str);
gchar**			li_ptr_array_to_strv (GPtrArray *array);
gchar			*li_str_replace (const gchar* str,
								const gchar* old_str,
								const gchar* new_str);

gboolean		li_copy_file (const gchar *source,
								const gchar *destination,
								GError **error);
gboolean		li_delete_dir_recursive (const gchar* dirname);
GPtrArray		*li_utils_find_files_matching (const gchar* dir,
												const gchar* pattern,
												gboolean recursive);
GPtrArray		*li_utils_find_files (const gchar* dir,
									gboolean recursive);
gchar			*li_utils_get_tmp_dir (const gchar *prefix);

gboolean		li_utils_is_root (void);

gchar			*li_compute_checksum_for_file (const gchar *fname);
gchar			*li_get_uuid_string (void);
const gchar		*li_get_last_version_from_component (AsComponent *cpt);

const gchar		*li_get_prefixdir (void);
void			li_set_unittestmode (gboolean testmode);
gboolean		li_get_unittestmode (void);

G_END_DECLS

#endif /* __LI_UTILS_PRIVATE_H */
