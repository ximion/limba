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

typedef struct _LiConfigDataPrivate	LiConfigDataPrivate;
struct _LiConfigDataPrivate
{
	GPtrArray *content;
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
