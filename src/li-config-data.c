/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:li-config-file
 * @short_description: Data format used by Limba to store package options
 */

#include "li-config-data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "li-utils.h"
#include "li-utils-private.h"

typedef struct _LiConfigDataPrivate	LiConfigDataPrivate;
struct _LiConfigDataPrivate
{
	GList *content;
	GList *current_block_pos;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiConfigData, li_config_data, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (li_config_data_get_instance_private (o))

/**
 * li_config_data_finalize:
 **/
static void
li_config_data_finalize (GObject *object)
{
	LiConfigData *cdata = LI_CONFIG_DATA (object);
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	g_list_free_full (priv->content, g_free);

	G_OBJECT_CLASS (li_config_data_parent_class)->finalize (object);
}

/**
 * li_config_data_init:
 **/
static void
li_config_data_init (LiConfigData *cdata)
{
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	priv->current_block_pos = NULL;
}

/**
 * li_config_data_load_data:
 */
void
li_config_data_load_data (LiConfigData *cdata, const gchar *data)
{
	gchar **lines = NULL;
	guint i;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	/* clear the content list */
	if (priv->content != NULL) {
		g_list_free_full (priv->content, g_free);
		priv->content = NULL;
	}

	lines = g_strsplit (data, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		priv->content = g_list_append (priv->content, g_strdup (lines[i]));
	}

	g_strfreev (lines);
}

/**
 * li_config_data_load_file:
 */
void
li_config_data_load_file (LiConfigData *cdata, GFile *file, GError **error)
{
	GError *tmp_error = NULL;
	g_autoptr(GFileInfo) info = NULL;
	const gchar *content_type = NULL;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	info = g_file_query_info (file,
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				G_FILE_QUERY_INFO_NONE,
				NULL, NULL);
	if (info != NULL)
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

	/* clear the list */
	if (priv->content != NULL) {
		g_list_free_full (priv->content, g_free);
		priv->content = NULL;
	}

	if ((g_strcmp0 (content_type, "application/gzip") == 0) || (g_strcmp0 (content_type, "application/x-gzip") == 0)) {
		GFileInputStream *fistream;
		GMemoryOutputStream *mem_os;
		GInputStream *conv_stream;
		GZlibDecompressor *zdecomp;

		guint8 *data;
		gchar **strv;
		guint i;

		/* load a GZip compressed file */

		fistream = g_file_read (file, NULL, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}

		mem_os = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (NULL, 0, g_realloc, g_free));
		zdecomp = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
		conv_stream = g_converter_input_stream_new (G_INPUT_STREAM (fistream), G_CONVERTER (zdecomp));
		g_object_unref (zdecomp);

		g_output_stream_splice (G_OUTPUT_STREAM (mem_os), conv_stream, 0, NULL, NULL);
		data = g_memory_output_stream_get_data (mem_os);
		if (data != NULL) {
			strv = g_strsplit ((const gchar*) data, "\n", -1);

			for (i = 0; strv[i] != NULL; i++) {
				priv->content = g_list_append (priv->content, g_strdup (strv[i]));
			}

			g_strfreev (strv);
		} else {
			g_debug ("Control file was empty (data == NULL)");
		}

		g_object_unref (conv_stream);
		g_object_unref (mem_os);
		g_object_unref (fistream);
	} else {
		g_autofree gchar *line = NULL;
		g_autofree gchar *fname = NULL;
		FILE *fstream;
		size_t len = 0;
		ssize_t read;

		/* We don't use GLib/GIO here sice this particular function is used after
		 * pivoting into the build virtual env, where the code hangs at g_data_input_stream_new ()
		 * for unknown reasons (it's probably trying to access some GIO stuff inside the constructor,
		 * which is loaded from the venv and mismatches with the stuff we have in memory from the host.
		 * FIXME: This is an ugly workaround, a proper solution needs to be found to make this beautiful
		 * again.
		 */

		fname = g_file_get_path (file);
		g_assert (fname != NULL);

		fstream = fopen (fname, "r");
		if (fstream == NULL) {
			g_set_error (error,
					G_IO_ERROR,
					G_IO_ERROR_FAILED,
					"Unable to open file '%s' for reading", fname);
			return;
		}

		while ((read = getline (&line, &len, fstream)) != -1) {
			g_strchomp (line);
			priv->content = g_list_append (priv->content, g_strdup (line));
		}

		fclose (fstream);
	}
}

/**
 * li_config_data_reset:
 *
 * Reset current block index and jup to the beginning.
 */
void
li_config_data_reset (LiConfigData *cdata)
{
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);
	priv->current_block_pos = NULL;
}

/**
 * li_line_empty:
 *
 * Check if line is empty or a comment
 */
static gboolean
li_line_empty (const gchar *line)
{
	if (li_str_empty (line))
		return TRUE;
	return g_str_has_prefix (line, "#");
}

/**
 * li_config_data_open_block:
 * @cdata: A valid #LiConfigData instance
 * @field: A field indentifier
 * @value: (allow-none) (default NULL): The value of the field, or %NULL if not important
 * @reset_index: %TRUE if the block should be searched from the beginning, or from the current position.
 *
 * Open a block in the config file.
 */
gboolean
li_config_data_open_block (LiConfigData *cdata, const gchar *field, const gchar *value, gboolean reset_index)
{
	GList *l;
	GList *block_pos;
	gboolean start = FALSE;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (reset_index)
		li_config_data_reset (cdata);

	if (priv->content == NULL) {
		li_config_data_reset (cdata);
		return FALSE;
	}

	block_pos = priv->content;
	if (priv->current_block_pos == NULL) {
		priv->current_block_pos = priv->content;
		start = TRUE;
	}

	for (l = priv->current_block_pos; l != NULL; l = l->next) {
		gchar *line;
		gchar *field_data;

		line = (gchar*) l->data;
		if (li_line_empty (line)) {
			start = TRUE;
			if (l->next == NULL)
				return FALSE;
			block_pos = l->next;
		}

		if (!start)
			continue;

		if (value == NULL) {
			field_data = g_strdup_printf ("%s:", field);
			if (g_str_has_prefix (line, field_data)) {
				priv->current_block_pos = block_pos;
				g_free (field_data);
				return TRUE;
			}
			g_free (field_data);
		} else {
			field_data = g_strdup_printf ("%s: %s", field, value);
			if (g_strcmp0 (line, field_data) == 0) {
				priv->current_block_pos = block_pos;
				g_free (field_data);
				return TRUE;
			}
			g_free (field_data);
		}
	}

	priv->current_block_pos = NULL;
	return FALSE;
}

/**
 * li_config_data_get_value:
 */
gchar*
li_config_data_get_value (LiConfigData *cdata, const gchar *field)
{
	GString *res;
	GList *l;
	GList *start_pos;
	gboolean add_to_value = FALSE;
	gboolean found = FALSE;
	gchar *tmp_str;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (priv->content == NULL) {
		return NULL;
	}

	start_pos = priv->current_block_pos;
	if (start_pos == NULL)
		start_pos = priv->content;

	res = g_string_new ("");
	for (l = start_pos; l != NULL; l = l->next) {
		gchar *line;
		gchar *field_data;

		line = (gchar*) l->data;
		if (li_str_empty (line)) {
			/* check if we should continue although we have reached the end of this block */
			if (priv->current_block_pos == NULL) {
				/* a "NULL position" means no block was opened previously, so we continue in that case
				 and break otherwise */
				continue;
			} else {
				break;
			}
		}

		if (add_to_value) {
			if (g_str_has_prefix (line, " ")) {
				gchar *str;
				str = g_strdup (line);
				g_strstrip (str);
				g_string_append_printf (res, "\n%s", str);
				g_free (str);
				found = TRUE;
			} else {
				break;
			}
		}

		field_data = g_strdup_printf ("%s:", field);
		if (g_str_has_prefix (line, field_data)) {
			gchar **tmp;
			tmp = g_strsplit (line, ":", 2);
			g_strstrip (tmp[1]);
			g_string_append (res, tmp[1]);
			g_strfreev (tmp);
			add_to_value = TRUE;
			found = TRUE;
		}
		g_free (field_data);

	};

	if (!found) {
		/* we did not find the field */
		g_string_free (res, TRUE);
		return NULL;
	}

	tmp_str = g_string_free (res, FALSE);
	g_strstrip (tmp_str);
	if (li_str_empty (tmp_str)) {
		g_free (tmp_str);
		return NULL;
	}

	return tmp_str;
}

/**
 * li_config_data_set_value:
 * @field: The field which should be changed
 * @value: The new value for the specified field
 *
 * Change the value of a field in the currently opened block.
 * If the field does not exist, it will be created.
 *
 * Returns: %TRUE if value was sucessfully changed.
 */
gboolean
li_config_data_set_value (LiConfigData *cdata, const gchar *field, const gchar *value)
{
	gchar **value_lines;
	gchar *tmp;
	GList *l;
	GList *start_pos;
	g_autofree gchar *field_str = NULL;
	gchar *field_data;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (field == NULL)
		return FALSE;
	if (value == NULL)
		return FALSE;

	/* don't trust the current position pointer */
	if (priv->content == NULL)
		priv->current_block_pos = NULL;

	start_pos = priv->current_block_pos;
	if (start_pos == NULL)
		start_pos = priv->content;

	value_lines = g_strsplit (value, "\n", -1);
	tmp = g_strjoinv ("\n ", value_lines);
	field_str = g_strdup_printf ("%s:", field);
	field_data = g_strdup_printf ("%s: %s", field, tmp);
	g_free (tmp);
	g_strfreev (value_lines);

	if (start_pos == g_list_last (priv->content)) {
		/* handle new block starts immediately */
		priv->content = g_list_append (priv->content, field_data);

		/* did we open a new block? */
		if (start_pos == NULL) {
			priv->current_block_pos = priv->content;
		} else if (li_str_empty ((gchar*) start_pos->data)) {
			priv->current_block_pos = g_list_last (priv->content);
		}

		return TRUE;
	}

	for (l = start_pos; l != NULL; l = l->next) {
		gchar *line;

		line = (gchar*) l->data;
		if (li_str_empty (line)) {
			/* check if we should continue although we have reached the end of this block */
			if (priv->current_block_pos == NULL) {
				/* an empty current block position means no block was opened previously, so we continue in that case
				 and break otherwise */
				continue;
			} else {
				/* we can add data to this block and exit */
				priv->content = g_list_insert_before (priv->content, l, field_data);
				return TRUE;
			}
		}
		if (g_str_has_prefix (line, field_str)) {
			/* field already exists, replace it */
			priv->content = g_list_insert_before (priv->content, l, field_data);
			priv->content = g_list_remove_link (priv->content, l);
			g_free (l->data);
			g_list_free (l);

			/* we're done here */
			return TRUE;
		}
	}

	/* if we are here, we can just append the new data to the end of the list */
	priv->content = g_list_append (priv->content, field_data);

	return TRUE;
}

/**
 * li_config_data_get_data:
 */
gchar*
li_config_data_get_data (LiConfigData *cdata)
{
	GString *res;
	GList *l;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (priv->content == NULL) {
		return g_strdup ("");
	}

	res = g_string_new ("");
	for (l = priv->content; l != NULL; l = l->next) {
		gchar *line;
		line = (gchar*) l->data;
		g_string_append_printf (res, "%s\n", line);
	}

	return g_string_free (res, FALSE);
}

/**
 * li_config_data_new_block:
 *
 * Create a new block at the end of the file and open it.
 */
void
li_config_data_new_block (LiConfigData *cdata)
{
	GList *l;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (priv->content == NULL) {
		li_config_data_reset (cdata);
		return;
	}

	l = g_list_last (priv->content);
	if (!li_line_empty ((gchar*) l->data)) {
		priv->content = g_list_append (priv->content, g_strdup (""));
	}
	priv->current_block_pos = g_list_last (priv->content);
}

/**
 * li_config_data_next:
 * @cdata: A valid #LiConfigData instance
 *
 * Jump to the next block.
 *
 * Returns: %TRUE if successful, %FALSE if there is no new block to be found.
 */
gboolean
li_config_data_next (LiConfigData *cdata)
{
	GList *l;
	GList *start_pos;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (priv->content == NULL) {
		li_config_data_reset (cdata);
		return FALSE;
	}

	start_pos = priv->current_block_pos;
	if (start_pos == NULL)
		start_pos = priv->content;

	for (l = start_pos; l != NULL; l = l->next) {
		gchar *line;

		line = (gchar*) l->data;
		if (li_line_empty (line)) {
			if (l->next == NULL)
				return FALSE;
			priv->current_block_pos = l->next;
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * li_config_data_save_to_file:
 */
gboolean
li_config_data_save_to_file (LiConfigData *cdata, const gchar *filename, GError **error)
{
	GFile *file;
	GError *tmp_error = NULL;
	gboolean ret = FALSE;
	gchar *data = NULL;

	file = g_file_new_for_path (filename);

	data = li_config_data_get_data (cdata);

	if (g_str_has_suffix (filename, ".gz")) {
		g_autoptr(GOutputStream) out2 = NULL;
		g_autoptr(GOutputStream) out = NULL;
		g_autoptr(GZlibCompressor) compressor = NULL;

		/* write a gzip compressed file */
		compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
		out = g_memory_output_stream_new_resizable ();
		out2 = g_converter_output_stream_new (out, G_CONVERTER (compressor));

		if (!g_output_stream_write_all (out2, data, strlen (data),
					NULL, NULL, &tmp_error)) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

		g_output_stream_close (out2, NULL, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

		if (!g_file_replace_contents (file,
			g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (out)),
						g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (out)),
						NULL,
						FALSE,
						G_FILE_CREATE_NONE,
						NULL,
						NULL,
						&tmp_error)) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

	} else {
		g_autoptr(GFileOutputStream) fos = NULL;
		g_autoptr(GDataOutputStream) dos = NULL;

		/* write uncompressed file */
		if (g_file_query_exists (file, NULL)) {
			fos = g_file_replace (file,
							NULL,
							FALSE,
							G_FILE_CREATE_REPLACE_DESTINATION,
							NULL,
							&tmp_error);
		} else {
			fos = g_file_create (file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &tmp_error);
		}
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

		dos = g_data_output_stream_new (G_OUTPUT_STREAM (fos));
		g_data_output_stream_put_string (dos, data, NULL, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}
	}

	ret = TRUE;
out:
	g_object_unref (file);
	if (data != NULL)
		g_free (data);

	return ret;
}

/**
 * li_config_data_class_init:
 **/
static void
li_config_data_class_init (LiConfigDataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_config_data_finalize;
}

/**
 * li_config_data_new:
 *
 * Creates a new #LiConfigData.
 *
 * Returns: (transfer full): a #LiConfigData
 *
 **/
LiConfigData *
li_config_data_new (void)
{
	LiConfigData *cdata;
	cdata = g_object_new (LI_TYPE_CONFIG_DATA, NULL);
	return LI_CONFIG_DATA (cdata);
}
