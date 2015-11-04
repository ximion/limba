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

#define LI_TYPE_REPO_ENTRY		(li_repo_entry_get_type())
#define LI_REPO_ENTRY(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_REPO_ENTRY, LiRepoEntry))
#define LI_REPO_ENTRY_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_REPO_ENTRY, LiRepoEntryClass))
#define LI_IS_REPO_ENTRY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_REPO_ENTRY))
#define LI_IS_REPO_ENTRY_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_REPO_ENTRY))
#define LI_REPO_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_REPO_ENTRY, LiRepoEntryClass))

G_BEGIN_DECLS

typedef struct _LiRepoEntry		LiRepoEntry;
typedef struct _LiRepoEntryClass	LiRepoEntryClass;

struct _LiRepoEntry
{
	GObject			parent;
};

struct _LiRepoEntryClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (LiRepoEntry, g_object_unref)

/**
 * LiRepoEntryKinds:
 * @LI_REPO_ENTRY_KIND_NONE:		Source entry has no kind (= it is disabled)
 * @LI_REPO_ENTRY_KIND_COMMON:		Download the "common" index
 * @LI_REPO_ENTRY_KIND_DEVEL:		Download the "devel" (SDK) index
 * @LI_REPO_ENTRY_KIND_SOURCE:		Download the "source" index
 *
 * Flags defining which index kinds this repository entry defines.
 **/
typedef enum  {
	LI_REPO_ENTRY_KIND_NONE = 0,
	LI_REPO_ENTRY_KIND_COMMON = 1 << 0,
	LI_REPO_ENTRY_KIND_DEVEL = 1 << 1,
	LI_REPO_ENTRY_KIND_SOURCE = 1 << 2
} LiRepoEntryKinds;

const gchar		*li_repo_entry_kind_to_string (LiRepoEntryKinds kind);
LiRepoEntryKinds	li_repo_entry_kind_from_string (const gchar *kind_str);

GType			li_repo_entry_get_type (void);
LiRepoEntry		*li_repo_entry_new (void);

gboolean		li_repo_entry_parse (LiRepoEntry *re,
						const gchar *repo_line);

LiRepoEntryKinds	li_repo_entry_get_kinds (LiRepoEntry *re);
void			li_repo_entry_set_kinds (LiRepoEntry *re,
						 LiRepoEntryKinds kinds);
void			li_repo_entry_add_kind (LiRepoEntry *re,
						LiRepoEntryKinds kind);
gboolean		li_repo_entry_has_kind (LiRepoEntry *re,
						LiRepoEntryKinds kind);

const gchar		*li_repo_entry_get_url (LiRepoEntry *re);
void			li_repo_entry_set_url (LiRepoEntry *re,
						const gchar *url);

G_END_DECLS

#endif /* __LI_REPO_ENTRY_H */
