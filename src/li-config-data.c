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
 * @short_description: Data format used by Listaller to store package options
 */

#include "li-config-data.h"

#include "li-utils.h"

typedef struct _LiConfigDataPrivate	LiConfigDataPrivate;
struct _LiConfigDataPrivate
{
	GPtrArray *content;
	guint current_block_id;
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

	g_ptr_array_unref (priv->content);

	G_OBJECT_CLASS (li_config_data_parent_class)->finalize (object);
}

/**
 * li_config_data_init:
 **/
static void
li_config_data_init (LiConfigData *cdata)
{
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	priv->content = g_ptr_array_new_with_free_func (g_free);
	priv->current_block_id = 0;
}

/**
 * li_config_data_load_file:
 */
void
li_config_data_load_file (LiConfigData *cdata, GFile *file)
{
	gchar* line = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	ir = g_file_read (file, NULL, NULL);
	dis = g_data_input_stream_new ((GInputStream*) ir);
	g_object_unref (ir);

	/* clear the array */
	if (priv->content->len > 0)
		g_ptr_array_remove_range (priv->content, 0, priv->content->len);

	while (TRUE) {
		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		g_ptr_array_add (priv->content, line);
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
	priv->current_block_id = 0;
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
	guint block_id;
	gboolean start = FALSE;
	LiConfigDataPrivate *priv = GET_PRIVATE (cdata);

	if (reset_index)
		li_config_data_reset (cdata);

	if (priv->content->len == 0) {
		li_config_data_reset (cdata);
		return FALSE;
	}

	if (priv->current_block_id == 0)
		start = TRUE;

	for (i = 0; i < priv->content->len; i++) {
		gchar *line;
		gchar *field_name;
		if (i < priv->current_block_id)
			continue;
		line = (gchar*) g_ptr_array_index (priv->content, i);

		if (li_str_empty (line)) {
			start = TRUE;
			block_id = i + 1;
		}

		if (!start)
			continue;

		field_name = g_strdup_printf ("%s:", field);
		if (g_str_has_prefix (line, field_name)) {
			priv->current_block_id = block_id;
			g_free (field_name);
			return TRUE;
		}
		g_free (field_name);
	}

	priv->current_block_id = 0;

	return FALSE;
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
