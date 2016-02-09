/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __LI_INSTALLER_PRIVATE_H
#define __LI_INSTALLER_PRIVATE_H

#include <glib-object.h>
#include "li-installer.h"

G_BEGIN_DECLS

gboolean		li_installer_install_sourcepkg_deps (LiInstaller *inst,
								LiPkgInfo *spki,
								GError **error);
void			li_installer_open_extra_packages (LiInstaller *inst,
								GPtrArray *files,
								GError **error);

G_END_DECLS

#endif /* __LI_INSTALLER_PRIVATE_H */
