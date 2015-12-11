/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __LI_PKG_CACHE_H
#define __LI_PKG_CACHE_H

#include <glib-object.h>
#include "li-pkg-info.h"

G_BEGIN_DECLS

#define LI_TYPE_PKG_CACHE (li_pkg_cache_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiPkgCache, li_pkg_cache, LI, PKG_CACHE, GObject)

struct _LiPkgCacheClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
};

/**
 * LiPkgCacheError:
 * @LI_PKG_CACHE_ERROR_FAILED:			Generic failure
 * @LI_PKG_CACHE_ERROR_DOWNLOAD_FAILED:		Failed to download a file
 * @LI_PKG_CACHE_ERROR_REMOTE_NOT_FOUND:	Data could not be found on remote server
 * @LI_PKG_CACHE_ERROR_WRITE:			Writing to cache was not possible
 * @LI_PKG_CACHE_ERROR_NOT_FOUND:		A cache entity was not found
 * @LI_PKG_CACHE_ERROR_VERIFICATION:		Validation of repository data failed.
 * @LI_PKG_CACHE_ERROR_UNPACK:			Unpacking of data (e.g. from the icon tarball) has failed.
 *
 * The error type.
 **/
typedef enum {
	LI_PKG_CACHE_ERROR_FAILED,
	LI_PKG_CACHE_ERROR_DOWNLOAD_FAILED,
	LI_PKG_CACHE_ERROR_REMOTE_NOT_FOUND,
	LI_PKG_CACHE_ERROR_NOT_FOUND,
	LI_PKG_CACHE_ERROR_WRITE,
	LI_PKG_CACHE_ERROR_VERIFICATION,
	LI_PKG_CACHE_ERROR_UNPACK,
	/*< private >*/
	LI_PKG_CACHE_ERROR_LAST
} LiPkgCacheError;

#define	LI_PKG_CACHE_ERROR li_pkg_cache_error_quark ()
GQuark li_pkg_cache_error_quark (void);

LiPkgCache		*li_pkg_cache_new (void);

void			li_pkg_cache_open (LiPkgCache *cache,
						GError **error);
void			li_pkg_cache_update (LiPkgCache *cache,
						GError **error);

GPtrArray		*li_pkg_cache_get_packages (LiPkgCache *cache);
LiPkgInfo		*li_pkg_cache_get_pkg_info (LiPkgCache *cache,
							const gchar *pkid);

gchar			*li_pkg_cache_fetch_remote (LiPkgCache *cache,
							const gchar *pkgid,
							GError **error);

G_END_DECLS

#endif /* __LI_PKG_CACHE_H */
