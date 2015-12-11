/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __LI_REPO_ENTRY_H
#define __LI_REPO_ENTRY_H

#include <glib-object.h>
#include "li-pkg-info.h"

G_BEGIN_DECLS

#define LI_TYPE_REPO_ENTRY (li_repo_entry_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiRepoEntry, li_repo_entry, LI, REPO_ENTRY, GObject)

struct _LiRepoEntryClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
};

/* default locations */
#define LIMBA_CACHE_DIR "/var/cache/limba/"
#define APPSTREAM_CACHE_DIR "/var/cache/app-info/"

/**
 * LiRepoIndexKinds:
 * @LI_REPO_INDEX_KIND_NONE:		Source entry has no kind (= it is disabled)
 * @LI_REPO_INDEX_KIND_COMMON:		Download the "common" index
 * @LI_REPO_INDEX_KIND_DEVEL:		Download the "devel" (SDK) index
 * @LI_REPO_INDEX_KIND_SOURCE:		Download the "source" index
 *
 * Flags defining which index kinds this repository entry defines.
 **/
typedef enum  {
	LI_REPO_INDEX_KIND_NONE = 0,
	LI_REPO_INDEX_KIND_COMMON = 1 << 0,
	LI_REPO_INDEX_KIND_DEVEL = 1 << 1,
	LI_REPO_INDEX_KIND_SOURCE = 1 << 2
} LiRepoIndexKinds;

const gchar		*li_repo_index_kind_to_string (LiRepoIndexKinds kind);
LiRepoIndexKinds	li_repo_index_kind_from_string (const gchar *kind_str);


LiRepoEntry		*li_repo_entry_new (void);

gboolean		li_repo_entry_parse (LiRepoEntry *re,
						const gchar *repo_line);

LiRepoIndexKinds	li_repo_entry_get_kinds (LiRepoEntry *re);
void			li_repo_entry_set_kinds (LiRepoEntry *re,
						 LiRepoIndexKinds kinds);
void			li_repo_entry_add_kind (LiRepoEntry *re,
						LiRepoIndexKinds kind);
gboolean		li_repo_entry_has_kind (LiRepoEntry *re,
						LiRepoIndexKinds kind);

const gchar		*li_repo_entry_get_url (LiRepoEntry *re);
void			li_repo_entry_set_url (LiRepoEntry *re,
						const gchar *url);

const gchar		*li_repo_entry_get_id (LiRepoEntry *re);
const gchar		*li_repo_entry_get_cache_dir (LiRepoEntry *re);
const gchar		*li_repo_entry_get_appstream_fname (LiRepoEntry *re);

gchar			**li_repo_entry_get_index_urls_for_arch (LiRepoEntry *re,
								 const gchar *arch);
gchar			*li_repo_entry_get_metadata_url_for_arch (LiRepoEntry *re,
								  const gchar *arch);

G_END_DECLS

#endif /* __LI_REPO_ENTRY_H */
