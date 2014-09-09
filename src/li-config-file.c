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

#include "config.h"
#include "li-config-file.h"

typedef struct _LiConfigFilePrivate	LiConfigFilePrivate;
struct _LiConfigFilePrivate
{
	GPtrArray *content;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiConfigFile, li_config_file, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_config_file_get_instance_private (o))

/**
 * li_config_file_finalize:
 **/
static void
li_config_file_finalize (GObject *object)
{
	LiConfigFile *lcf = LI_CONFIG_FILE (object);
	LiConfigFilePrivate *priv = GET_PRIVATE (lcf);

	g_ptr_array_unref (priv->content);

	G_OBJECT_CLASS (li_config_file_parent_class)->finalize (object);
}

/**
 * li_config_file_init:
 **/
static void
li_config_file_init (LiConfigFile *lcf)
{
	LiConfigFilePrivate *priv = GET_PRIVATE (lcf);

	priv->content = g_ptr_array_new_with_free_func (g_free);
}

/**
 * li_config_file_class_init:
 **/
static void
li_config_file_class_init (LiConfigFileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_config_file_finalize;
}

/**
 * li_config_file_new:
 *
 * Creates a new #LiConfigFile.
 *
 * Returns: (transfer full): a #LiConfigFile
 *
 **/
LiConfigFile *
li_config_file_new (void)
{
	LiConfigFile *lcf;
	lcf = g_object_new (LI_TYPE_CONFIG_FILE, NULL);
	return LI_CONFIG_FILE (lcf);
}
