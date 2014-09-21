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

#if !defined (__LISTALLER_H) && !defined (LI_COMPILATION)
#error "Only <listaller.h> can be included directly."
#endif

#ifndef __LI_FILE_ENTRY_H
#define __LI_FILE_ENTRY_H

#include <glib-object.h>

#define LI_TYPE_FILE_ENTRY			(li_file_entry_get_type())
#define LI_FILE_ENTRY(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_FILE_ENTRY, LiFileEntry))
#define LI_FILE_ENTRY_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_FILE_ENTRY, LiFileEntryClass))
#define LI_IS_FILE_ENTRY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_FILE_ENTRY))
#define LI_IS_FILE_ENTRY_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_FILE_ENTRY))
#define LI_FILE_ENTRY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_FILE_ENTRY, LiFileEntryClass))

G_BEGIN_DECLS

/**
 * LiFileEntryKind:
 * @AS_URL_KIND_UNKNOWN:			Type invalid or not known
 * @LI_FILE_ENTRY_KIND_FILE:		A file
 * @LI_FILE_ENTRY_KIND_DIRECTORY:	A directory
 *
 * The kind of a LiFileEntry
 **/
typedef enum {
	LI_FILE_ENTRY_KIND_UNKNOWN,
	LI_FILE_ENTRY_KIND_FILE,
	LI_FILE_ENTRY_KIND_DIRECTORY,
	AS_URL_KIND_LAST
} LiFileEntryKind;

typedef struct _LiFileEntry		LiFileEntry;
typedef struct _LiFileEntryClass	LiFileEntryClass;

struct _LiFileEntry
{
	GObject			parent;
};

struct _LiFileEntryClass
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

GType			li_file_entry_get_type	(void);
LiFileEntry		*li_file_entry_new		(void);

const gchar		*li_file_entry_get_fname (LiFileEntry *fe);
void			li_file_entry_set_fname (LiFileEntry *fe,
										 const gchar *fname);

const gchar		*li_file_entry_get_installed_location (LiFileEntry *fe);
void			li_file_entry_set_installed_location (LiFileEntry *fe,
											const gchar *fname);

const gchar		*li_file_entry_get_destination (LiFileEntry *fe);
void			li_file_entry_set_destination (LiFileEntry *fe,
											const gchar *dest);

const gchar		*li_file_entry_get_hash (LiFileEntry *fe);
void			li_file_entry_set_hash (LiFileEntry *fe,
										 const gchar *hash);

gchar			*li_file_entry_get_full_path (LiFileEntry *fe);
gboolean		li_file_entry_is_installed (LiFileEntry *fe);
gchar			*li_file_entry_to_string (LiFileEntry *fe);

guint			li_file_entry_hash_func (gconstpointer ptr);
gboolean		li_file_entry_equal_func (LiFileEntry *a,
										  LiFileEntry *b);

G_END_DECLS

#endif /* __LI_FILE_ENTRY_H */
