/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_CONFIG_DATA_H
#define __LI_CONFIG_DATA_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define LI_TYPE_CONFIG_DATA (li_config_data_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiConfigData, li_config_data, LI, CONFIG_DATA, GObject)

struct _LiConfigDataClass
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

LiConfigData		*li_config_data_new (void);

void			li_config_data_load_file (LiConfigData *cdata,
							GFile *file,
							GError **error);
void			li_config_data_load_data (LiConfigData *cdata,
							const gchar *data);
gboolean		li_config_data_open_block (LiConfigData *cdata,
							const gchar *field,
							const gchar *value,
							gboolean reset_index);

void			li_config_data_new_block (LiConfigData *cdata);

gchar			*li_config_data_get_value (LiConfigData *cdata,
							const gchar *field);
gboolean		li_config_data_set_value (LiConfigData *cdata,
							const gchar *field,
							const gchar *value);

gchar			*li_config_data_get_data (LiConfigData *cdata);
gboolean		li_config_data_save_to_file (LiConfigData *cdata,
							const gchar *filename,
							GError **error);

void			li_config_data_reset (LiConfigData *cdata);
gboolean		li_config_data_next (LiConfigData *cdata);

G_END_DECLS

#endif /* __LI_CONFIG_DATA_H */
