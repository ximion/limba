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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_MANAGER_H
#define __LI_MANAGER_H

#include <glib-object.h>
#include "li-runtime.h"

#define LI_TYPE_MANAGER			(li_manager_get_type())
#define LI_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_MANAGER, LiManager))
#define LI_MANAGER_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_MANAGER, LiManagerClass))
#define LI_IS_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_MANAGER))
#define LI_IS_MANAGER_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_MANAGER))
#define LI_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_MANAGER, LiManagerClass))

G_BEGIN_DECLS

/**
 * LiManagerError:
 * @LI_MANAGER_ERROR_FAILED:		Generic failure
 * @LI_MANAGER_ERROR_NOT_FOUND:		The software / item was not found
 * @LI_MANAGER_ERROR_DEPENDENCY:	Completing this action would break a dependent package
 * @LI_MANAGER_ERROR_REMOVE_FAILED:	Failed to remove a file
 *
 * The error type.
 **/
typedef enum {
	LI_MANAGER_ERROR_FAILED,
	LI_MANAGER_ERROR_NOT_FOUND,
	LI_MANAGER_ERROR_DEPENDENCY,
	LI_MANAGER_ERROR_REMOVE_FAILED,
	/*< private >*/
	LI_MANAGER_ERROR_LAST
} LiManagerError;

#define	LI_MANAGER_ERROR li_manager_error_quark ()
GQuark li_manager_error_quark (void);

typedef struct _LiManager		LiManager;
typedef struct _LiManagerClass	LiManagerClass;

struct _LiManager
{
	GObject			parent;
};

struct _LiManagerClass
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

GType			li_manager_get_type	(void);
LiManager		*li_manager_new		(void);

GList			*li_manager_get_software_list (LiManager *mgr,
											GError **error);
GPtrArray		*li_manager_get_installed_runtimes (LiManager *mgr);
LiPkgInfo		*li_manager_get_software_by_pkid (LiManager *mgr,
											const gchar *pkid,
											GError **error);

LiRuntime		*li_manager_find_runtime_with_members (LiManager *mgr,
													GPtrArray *members);

gboolean		li_manager_remove_software (LiManager *mgr,
											const gchar *pkgid,
											GError **error);

gboolean		li_manager_package_is_installed (LiManager *mgr,
												LiPkgInfo *pki);

gboolean		li_manager_cleanup (LiManager *mgr,
									GError **error);

void			li_manager_receive_key (LiManager *mgr,
									const gchar *fpr,
									GError **error);

void			li_manager_refresh_cache (LiManager *mgr,
									GError **error);

GList			*li_manager_get_update_list (LiManager *mgr,
									GError **error);

gboolean		li_manager_apply_updates (LiManager *mgr,
									GError **error);

G_END_DECLS

#endif /* __LI_MANAGER_H */
