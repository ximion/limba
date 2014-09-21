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

/**
 * SECTION:li-file-entry
 * @short_description: Information about a file in a Listaller package
 */

#include "config.h"
#include "li-file-entry.h"

typedef struct _LiFileEntryPrivate	LiFileEntryPrivate;
struct _LiFileEntryPrivate
{
	LiFileEntryKind kind;
	gchar *fname;
	gchar *destination;
	gchar *hash;
	gchar *fname_installed;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiFileEntry, li_file_entry, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_file_entry_get_instance_private (o))

/**
 * li_file_entry_get_kind:
 */
LiFileEntryKind
li_file_entry_get_kind (LiFileEntry *fe)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	return priv->kind;
}

/**
 * li_file_entry_set_kind:
 */
void
li_file_entry_set_kind (LiFileEntry *fe, LiFileEntryKind kind)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	priv->kind = kind;
}

/**
 * li_file_entry_get_fname:
 */
const gchar*
li_file_entry_get_fname (LiFileEntry *fe)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	return priv->fname;
}

/**
 * li_file_entry_set_fname:
 */
void
li_file_entry_set_fname (LiFileEntry *fe, const gchar *fname)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	g_free (priv->fname);
	priv->fname = g_strdup (fname);
}

/**
 * li_file_entry_get_installed_location:
 */
const gchar*
li_file_entry_get_installed_location (LiFileEntry *fe)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	return priv->fname_installed;
}

/**
 * li_file_entry_set_installed_location:
 */
void
li_file_entry_set_installed_location (LiFileEntry *fe, const gchar *fname)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	g_free (priv->fname_installed);
	priv->fname_installed = g_strdup (fname);
}

/**
 * li_file_entry_get_destination:
 */
const gchar*
li_file_entry_get_destination (LiFileEntry *fe)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	return priv->destination;
}

/**
 * li_file_entry_set_destination:
 */
void
li_file_entry_set_destination (LiFileEntry *fe, const gchar *dest)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	g_free (priv->destination);
	priv->destination = g_strdup (dest);
}

/**
 * li_file_entry_get_hash:
 */
const gchar*
li_file_entry_get_hash (LiFileEntry *fe)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	return priv->hash;
}

/**
 * li_file_entry_set_hash:
 */
void
li_file_entry_set_hash (LiFileEntry *fe, const gchar *hash)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	g_free (priv->hash);
	priv->hash = g_strdup (hash);
}

/**
 * li_file_entry_get_full_path:
 *
 * Get full path to the file before it was installed
 */
gchar*
li_file_entry_get_full_path (LiFileEntry *fe)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	return g_build_filename (priv->destination, priv->fname, NULL);
}

/**
 * li_file_entry_is_installed:
 *
 * Get full path to the file before it was installed
 */
gboolean
li_file_entry_is_installed (LiFileEntry *fe)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	if ((priv->fname_installed != NULL) && (g_strcmp0 (priv->fname_installed, "") != 0))
		return TRUE;
	return FALSE;
}

/**
 * li_file_entry_to_string:
 */
gchar*
li_file_entry_to_string (LiFileEntry *fe)
{
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);
	return g_strdup_printf ("<<FileEntry>> (%s) to (%s); hash: %s", priv->fname, priv->destination, priv->hash);
}

/**
 * li_file_entry_finalize:
 **/
static void
li_file_entry_finalize (GObject *object)
{
	LiFileEntry *fe = LI_FILE_ENTRY (object);
	LiFileEntryPrivate *priv = GET_PRIVATE (fe);

	g_free (priv->fname);
	g_free (priv->fname_installed);
	g_free (priv->destination);
	g_free (priv->hash);

	G_OBJECT_CLASS (li_file_entry_parent_class)->finalize (object);
}

/**
 * li_file_entry_init:
 **/
static void
li_file_entry_init (LiFileEntry *fe)
{
}

/**
 * li_file_entry_class_init:
 **/
static void
li_file_entry_class_init (LiFileEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_file_entry_finalize;
}

/**
 * li_file_entry_new:
 *
 * Creates a new #LiFileEntry.
 *
 * Returns: (transfer full): a #LiFileEntry
 *
 **/
LiFileEntry *
li_file_entry_new (void)
{
	LiFileEntry *fe;
	fe = g_object_new (LI_TYPE_FILE_ENTRY, NULL);
	return LI_FILE_ENTRY (fe);
}

/**
 * li_file_entry_hash_func:
 *
 * Hash function for #LiFileEntry objects
 */
guint
li_file_entry_hash_func (LiFileEntry *fe)
{
	guint res = 0U;
	gchar* str;
	g_return_val_if_fail (fe != NULL, 0U);

	str = li_file_entry_get_full_path (fe);
	res = g_str_hash (str);
	g_free (str);
	return res;
}

/**
 * li_file_entry_equal_func:
 *
 * Equality-checking function for #LiFileEntry objects
 */
gboolean
li_file_entry_equal_func (LiFileEntry *a, LiFileEntry *b)
{
	const gchar *fname_a;
	const gchar *fname_b;
	const gchar *dest_a;
	const gchar *dest_b;

	fname_a = li_file_entry_get_fname (a);
	fname_b = li_file_entry_get_fname (b);
	dest_a = li_file_entry_get_destination (a);
	dest_b = li_file_entry_get_destination (b);

	if ((g_strcmp0 (fname_a, fname_b) == 0) && (g_strcmp0 (dest_a, dest_b) == 0))
		return TRUE;
	return FALSE;
}
