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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_DAEMON_JOB_H
#define __LI_DAEMON_JOB_H

#include <glib-object.h>

#include "li-dbus-interface.h"
#include "limba.h"

G_BEGIN_DECLS

#define LI_TYPE_DAEMON_JOB		(li_daemon_job_get_type())
G_DECLARE_DERIVABLE_TYPE (LiDaemonJob, li_daemon_job, LI, DAEMON_JOB, GObject)

struct _LiDaemonJobClass
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

GType			li_daemon_job_get_type	(void);
LiDaemonJob		*li_daemon_job_new	(void);

gboolean		li_daemon_job_prepare (LiDaemonJob *job,
						LiProxyManager *mgr_bus);

void			li_daemon_job_run_refresh_cache (LiDaemonJob *job);

void			li_daemon_job_run_remove_package (LiDaemonJob *job,
						const gchar *pkid);

void			li_daemon_job_run_install (LiDaemonJob *job,
						const gchar *pkid);
void			li_daemon_job_run_install_local (LiDaemonJob *job,
						const gchar *fname);

void			li_daemon_job_run_update_all (LiDaemonJob *job);
void			li_daemon_job_run_update (LiDaemonJob *job,
						  const gchar *pkid);

gboolean		li_daemon_job_is_running (LiDaemonJob *job);


G_END_DECLS

#endif /* __LI_DAEMON_JOB_H */
