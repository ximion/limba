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
 * SECTION:li-config-file
 * @short_description: Data format used by Limba to store package options
 */

#include "li-config-data.h"

#include "li-utils.h"
#include "li-utils-private.h"

typedef struct _LiConfigDataPrivate	LiConfigDataPrivate;
struct _LiConfigDataPrivate
{
	GList *content;
	gint current_block_id;
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

	priv->current_block_id = -1;
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
li_config_data_load_file (LiConfigData *cdata, GFile *file)
{
	gchar *line = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	ir = g_file_read (file, NULL, NULL);
	dis = g_data_input_stream_new ((GInputStream*) ir);
	g_object_unref (ir);

	/* clear the array */
	if (priv->content != NULL) {
		g_list_free_full (priv->content, g_free);
		priv->content = NULL;
	}

	while (TRUE) {
		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		priv->content = g_list_append (priv->content, line);
	}

	g_object_unref (dis);
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
	priv->current_block_id = -1;
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
	guint i;
	GList *l;
	guint block_id;
	gboolean start = FALSE;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (reset_index)
		li_config_data_reset (cdata);

	if (priv->content == NULL) {
		li_config_data_reset (cdata);
		return FALSE;
	}

	if (priv->current_block_id <= 0) {
		block_id = 0;
		start = TRUE;
	}

	for (l = priv->content; l != NULL; l = l->next) {
		gchar *line;
		gchar *field_data;

		i = g_list_position (priv->content, l);
		if (((gint) i) < priv->current_block_id)
			continue;
		line = (gchar*) l->data;

		if (li_line_empty (line)) {
			start = TRUE;
			block_id = i + 1;
		}

		if (!start)
			continue;

		if (value == NULL) {
			field_data = g_strdup_printf ("%s:", field);
			if (g_str_has_prefix (line, field_data)) {
				priv->current_block_id = block_id;
				g_free (field_data);
				return TRUE;
			}
			g_free (field_data);
		} else {
			field_data = g_strdup_printf ("%s: %s", field, value);
			if (g_strcmp0 (line, field_data) == 0) {
				priv->current_block_id = block_id;
				g_free (field_data);
				return TRUE;
			}
			g_free (field_data);
		}
	}

	priv->current_block_id = -1;
	return FALSE;
}

/**
 * li_config_data_get_value:
 */
gchar*
li_config_data_get_value (LiConfigData *cdata, const gchar *field)
{
	GString *res;
	gint i;
	GList *l;
	gboolean add_to_value = FALSE;
	gboolean found = FALSE;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (priv->content == NULL) {
		return NULL;
	}

	res = g_string_new ("");
	for (l = priv->content; l != NULL; l = l->next) {
		gchar *line;
		gchar *field_data;

		i = g_list_position (priv->content, l);
		if (i < priv->current_block_id)
			continue;
		line = (gchar*) l->data;

		if (li_str_empty (line)) {
			/* check if we should continue although we have reached the end of this block */
			if (priv->current_block_id < 0) {
				/* a negative current block id means no block was opened previously, so we continue in that case
				 and break otherwise */
				continue;
			} else {
				break;
			}
		}

		if (add_to_value) {
			if (g_str_has_prefix (line, " ")) {
				g_strstrip (line);
				g_string_append_printf (res, "\n%s", line);
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
		return FALSE;
	}

	return g_string_free (res, FALSE);
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
	gint i;
	GList *l;
	_cleanup_free_ gchar *field_str;
	gchar *field_data;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (field == NULL)
		return FALSE;
	if (value == NULL)
		return FALSE;

	/* don't trost the current id pointer */
	if (priv->content == NULL)
		priv->current_block_id = -1;

	value_lines = g_strsplit (value, "\n", -1);
	tmp = g_strjoinv ("\n ", value_lines);
	field_str = g_strdup_printf ("%s:", field);
	field_data = g_strdup_printf ("%s: %s", field, tmp);
	g_free (tmp);

	for (l = priv->content; l != NULL; l = l->next) {
		gchar *line;

		i = g_list_position (priv->content, l);
		if (((int) i) < priv->current_block_id)
			continue;
		line = (gchar*) l->data;

		if (li_str_empty (line)) {
			/* check if we should continue although we have reached the end of this block */
			if (priv->current_block_id < 0) {
				/* a negative current block id means no block was opened previously, so we continue in that case
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
 * li_config_data_save_to_file:
 */
gboolean
li_config_data_save_to_file (LiConfigData *cdata, const gchar *filename, GError **error)
{
	GFile *file;
	GDataOutputStream *dos = NULL;
	GFileOutputStream *fos;
	GError *tmp_error = NULL;
	gboolean ret = FALSE;
	gchar *data = NULL;

	file = g_file_new_for_path (filename);

	fos = g_file_create (file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}
	dos = g_data_output_stream_new (G_OUTPUT_STREAM (fos));
	g_object_unref (fos);

	data = li_config_data_get_data (cdata);
	g_data_output_stream_put_string (dos, data, NULL, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	ret = TRUE;
out:
	g_object_unref (file);
	if (data != NULL)
		g_free (data);
	if (dos != NULL)
		g_object_unref (dos);

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
