/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015-2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __LI_BUILD_MASTER_H
#define __LI_BUILD_MASTER_H

#include <glib-object.h>
#include <sys/types.h>

#define LI_TYPE_BUILD_MASTER			(li_build_master_get_type())
#define LI_BUILD_MASTER(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_BUILD_MASTER, LiBuildMaster))
#define LI_BUILD_MASTER_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_BUILD_MASTER, LiBuildMasterClass))
#define LI_IS_BUILD_MASTER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_BUILD_MASTER))
#define LI_IS_BUILD_MASTER_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_BUILD_MASTER))
#define LI_BUILD_MASTER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_BUILD_MASTER, LiBuildMasterClass))

G_BEGIN_DECLS

typedef struct _LiBuildMaster		LiBuildMaster;
typedef struct _LiBuildMasterClass	LiBuildMasterClass;

struct _LiBuildMaster
{
	GObject			parent;
};

struct _LiBuildMasterClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
};

/**
 * LiBuildMasterError:
 * @LI_BUILD_MASTER_ERROR_FAILED:		Generic failure
 * @LI_BUILD_MASTER_ERROR_INIT:			Something bad happened while initailizing.
 * @LI_BUILD_MASTER_ERROR_NO_COMMANDS:		No commands to execute
 * @LI_BUILD_MASTER_ERROR_BUILD_DEP_MISSING:	A build dependency is missing
 * @LI_BUILD_MASTER_ERROR_STEP_FAILED:		A build step failed
 *
 * The error type.
 **/
typedef enum {
	LI_BUILD_MASTER_ERROR_FAILED,
	LI_BUILD_MASTER_ERROR_INIT,
	LI_BUILD_MASTER_ERROR_NO_COMMANDS,
	LI_BUILD_MASTER_ERROR_BUILD_DEP_MISSING,
	LI_BUILD_MASTER_ERROR_STEP_FAILED,
	/*< private >*/
	LI_BUILD_MASTER_ERROR_LAST
} LiBuildMasterError;

#define	LI_BUILD_MASTER_ERROR li_build_master_error_quark ()
GQuark li_build_master_error_quark (void);

GType			li_build_master_get_type (void);
LiBuildMaster		*li_build_master_new (void);

void			li_build_master_init_build (LiBuildMaster *bmaster,
							const gchar *dir,
							const gchar *chroot_orig,
							GError **error);

gint			li_build_master_run (LiBuildMaster *bmaster,
						GError **error);
gint			li_build_master_get_shell (LiBuildMaster *bmaster,
							GError **error);

void			li_build_master_set_build_user (LiBuildMaster *bmaster,
							uid_t uid);
void			li_build_master_set_build_group (LiBuildMaster *bmaster,
							gid_t gid);

G_END_DECLS

#endif /* __LI_BUILD_MASTER_H */
