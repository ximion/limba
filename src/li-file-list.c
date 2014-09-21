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

#include <config.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "li-utils.h"
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
 * li_file_list_append_data_from_file:
 */
static gboolean
li_file_list_append_data_from_file (LiFileList *flist, const gchar *fname)
{
	GFile *file;
	_cleanup_free_ gchar* line = NULL;
	_cleanup_free_ gchar *current_dir = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;
	LiFileEntry *fe;
	gboolean ret = FALSE;
	LiFileListPrivate *priv = GET_PRIVATE (flist);

	file = g_file_new_for_path (fname);
	if (!g_file_query_exists (file, NULL)) {
		g_warning (_("File '%s' doesn't exist."), fname);
		goto out;
	}

	ir = g_file_read (file, NULL, NULL);
	dis = g_data_input_stream_new ((GInputStream*) ir);
	g_object_unref (ir);

	while (TRUE) {
		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		/* ignore empty lines */
		if (li_str_empty (line))
			continue;

		/* ignore comments */
		if (g_str_has_prefix (line, "#"))
			continue;

		/* new directory? */
		if (g_str_has_prefix (line, ":: ")) {
			if (current_dir != NULL)
				g_free (current_dir);
			current_dir = g_strdup (&line[3]);
			continue;
		}

		/* do we have a directory? */
		if (current_dir == NULL)
			continue;

		fe = li_file_entry_new ();
		if (priv->has_hashes) {
			gchar **strv;
			gint strv_len;

			strv = g_strsplit (line, " ", 2);
			strv_len = g_strv_length (strv);
			if (strv_len < 2) {
				g_warning ("File list '%s' is broken: Could not find hash for '%s'", fname, line);
				g_strfreev (strv);
				goto out;
			}
			li_file_entry_set_hash (fe, strv[0]);
			li_file_entry_set_fname (fe, strv[1]);
			g_strfreev (strv);
		} else {
			li_file_entry_set_fname (fe, line);
		}
		li_file_entry_set_destination (fe, current_dir);

		/* add the new file entry */
		g_hash_table_insert (priv->list,
							li_file_entry_get_full_path (fe),
							fe);
	}

	ret = TRUE;

out:
	g_object_unref (file);
	g_object_unref (dis);

	return ret;
}

/**
 * li_file_list_open_file:
 */
gboolean
li_file_list_open_file (LiFileList *flist, const gchar *fname)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);

	g_hash_table_remove_all (priv->list);
	return li_file_list_append_data_from_file (flist, fname);
}

/**
 * li_file_list_get_files:
 *
 * Returns: (transfer container): A list of #LiFileEntry objects
 */
GList*
li_file_list_get_files (LiFileList *flist)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);

	return g_hash_table_get_values (priv->list);
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
	g_free (priv->comment);
	g_free (priv->root_dir);

	G_OBJECT_CLASS (li_file_list_parent_class)->finalize (object);
}

/**
 * li_file_list_init:
 **/
static void
li_file_list_init (LiFileList *flist)
{
	LiFileListPrivate *priv = GET_PRIVATE (flist);

	priv->list = g_hash_table_new_full (g_str_hash,
						g_str_equal,
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
