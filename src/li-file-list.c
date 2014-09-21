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
 * SECTION:li-file-list
 * @short_description: Implementation of an IPK file listing
 */

#include "li-file-list.h"

#include <glib.h>
#include "li-file-entry.h"

typedef struct _LiFileListPrivate	LiFileListPrivate;
struct _LiFileListPrivate
{
	GHashTable *list;
	gchar *comment;
	gchar *root_dir;
	gboolean has_hashes;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiFileList, li_file_list, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_file_list_get_instance_private (o))

/**
 * li_file_list_get_comment:
 */
const gchar*
li_file_list_get_comment (LiFileList *flist)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);
	return priv->comment;
}

/**
 * li_file_list_set_comment:
 */
void
li_file_list_set_comment (LiFileList *flist, const gchar *comment)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);
	g_free (priv->comment);
	priv->comment = g_strdup (comment);
}

/**
 * li_file_list_get_root_dir:
 */
const gchar*
li_file_list_get_root_dir (LiFileList *flist)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);
	return priv->root_dir;
}

/**
 * li_file_list_set_root_dir:
 */
void
li_file_list_set_root_dir (LiFileList *flist, const gchar *root_dir)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);
	g_free (priv->root_dir);
	priv->root_dir = g_strdup (root_dir);
}

/**
 * li_file_list_has_hashes:
 */
gboolean
li_file_list_has_hashes (LiFileList *flist)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);
	return priv->has_hashes;
}

/**
 * li_file_list_finalize:
 **/
static void
li_file_list_finalize (GObject *object)
{
	LiFileList *flist = LI_FILE_LIST (object);
	LiFileListPrivate *priv = GET_PRIVATE (flist);

	g_hash_table_unref (priv->list);

	G_OBJECT_CLASS (li_file_list_parent_class)->finalize (object);
}

/**
 * li_file_list_init:
 **/
static void
li_file_list_init (LiFileList *flist)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);

	priv->list = g_hash_table_new_full ((GHashFunc) li_file_entry_hash_func,
						(GEqualFunc) li_file_entry_equal_func,
						g_free,
						(GDestroyNotify) g_object_unref);
	priv->comment = g_strdup ("IPK file list");
	priv->root_dir = g_get_current_dir ();
}

/**
 * li_file_list_class_init:
 **/
static void
li_file_list_class_init (LiFileListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_file_list_finalize;
}

/**
 * li_file_list_new:
 *
 * Creates a new #LiFileList.
 *
 * Returns: (transfer full): a #LiFileList
 *
 **/
LiFileList *
li_file_list_new (gboolean with_hashes)
{
	LiFileList *flist;
	LiFileListPrivate *priv;

	flist = g_object_new (LI_TYPE_FILE_LIST, NULL);
	priv = GET_PRIVATE (flist);
	priv->has_hashes = with_hashes;

	return LI_FILE_LIST (flist);
}
