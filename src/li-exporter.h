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

#ifndef __LI_EXPORTER_H
#define __LI_EXPORTER_H

#include <glib-object.h>
#include "li-pkg-info.h"

G_BEGIN_DECLS

#define LI_TYPE_EXPORTER (li_exporter_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiExporter, li_exporter, LI, EXPORTER, GObject)

struct _LiExporterClass
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

LiExporter		*li_exporter_new (void);

gboolean		li_exporter_process_file (LiExporter *exp,
							const gchar *filename,
							const gchar *disk_location,
							GError **error);

gboolean		li_exporter_get_override_allowed (LiExporter *exp);
void			li_exporter_set_override_allowed (LiExporter *exp,
								gboolean override);

LiPkgInfo		*li_exporter_get_pkg_info (LiExporter *exp);
void			li_exporter_set_pkg_info (LiExporter *exp,
							LiPkgInfo *pki);

gchar			*li_exporter_get_exported_files_index (LiExporter *exp);

G_END_DECLS

#endif /* __LI_EXPORTER_H */
