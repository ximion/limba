/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2015 Matthias Klumpp <matthias@tenstral.net>
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

typedef struct _AsComponent	AsComponent;

#define LI_GPG_PROTOCOL GPGME_PROTOCOL_OpenPGP

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
gchar			*li_get_pkgname_from_component (AsComponent *cpt);

void			li_add_to_new_scope (const gchar *domain,
						const gchar *idname,
						GError **error);

gchar			*li_env_get_user_fullname (void);
gchar			*li_env_get_user_email (void);
gchar			*li_env_get_target_repo (void);
void			li_env_set_user_details (const gchar *user_name,
							const gchar *user_email,
							const gchar *target_repo);

G_END_DECLS

#endif /* __LI_UTILS_PRIVATE_H */
