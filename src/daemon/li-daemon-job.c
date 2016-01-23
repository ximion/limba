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

/**
 * SECTION:li-daemon-job
 * @short_description: A install/remove job to be performed by the Limba helper daemon
 */

#include "config.h"
#include "li-daemon-job.h"

typedef enum {
	LI_JOB_KIND_NONE,
	LI_JOB_KIND_REFRESH_CACHE,
	LI_JOB_KIND_INSTALL,
	LI_JOB_KIND_INSTALL_LOCAL,
	LI_JOB_KIND_REMOVE,
	/*< private >*/
	LI_JOB_KIND_LAST
} LiJobKind;

typedef struct _LiDaemonJobPrivate	LiDaemonJobPrivate;
struct _LiDaemonJobPrivate
{
	LiProxyManager *mgr_bus;
	GThread *thread;
	LiJobKind kind;
	gboolean running;

	gchar *pkid;
	gchar *local_fname;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiDaemonJob, li_daemon_job, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_daemon_job_get_instance_private (o))

/**
 * li_daemon_job_finalize:
 **/
static void
li_daemon_job_finalize (GObject *object)
{
	LiDaemonJob *job = LI_DAEMON_JOB (object);
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	if (priv->mgr_bus != NULL)
		g_object_unref (priv->mgr_bus);
	if (priv->pkid != NULL)
		g_free (priv->pkid);
	if (priv->local_fname != NULL)
		g_free (priv->local_fname);

	G_OBJECT_CLASS (li_daemon_job_parent_class)->finalize (object);
}

/**
 * li_daemon_job_init:
 **/
static void
li_daemon_job_init (LiDaemonJob *job)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	priv->running = FALSE;
	priv->kind = LI_JOB_KIND_NONE;
}

/**
 * li_daemon_job_progress_proxy_cb:
 */
static void
li_daemon_job_progress_proxy_cb (LiManager *mgr, guint percentage, const gchar *id, LiProxyManager *mgr_bus)
{
	if (id == NULL)
		id = "";
	li_proxy_manager_emit_progress (mgr_bus, id, percentage);
}

/**
 * li_daemon_job_emit_error:
 */
static void
li_daemon_job_emit_error (LiDaemonJob *job, GError *error)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	if (error == NULL)
		return;

	li_proxy_manager_emit_error (priv->mgr_bus,
					error->domain,
					error->code,
					error->message);
	g_error_free (error);
}

/**
 * li_daemon_job_emit_finished:
 */
static void
li_daemon_job_emit_finished (LiDaemonJob *job, gboolean success)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);
	li_proxy_manager_emit_finished (priv->mgr_bus, success);
}

/**
 * li_daemon_job_execute_refresh_cache:
 */
static gboolean
li_daemon_job_execute_refresh_cache (LiDaemonJob *job)
{
	g_autoptr(LiManager) mgr = NULL;
	GError *error = NULL;
	gboolean ret = FALSE;
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	mgr = li_manager_new ();
	g_signal_connect (mgr, "progress",
			G_CALLBACK (li_daemon_job_progress_proxy_cb), priv->mgr_bus);

	li_manager_refresh_cache (mgr, &error);
	if (error != NULL) {
		li_daemon_job_emit_error (job, error);
		goto out;
	}

	ret = TRUE;
out:
	li_daemon_job_emit_finished (job, ret);
	return ret;
}

/**
 * li_daemon_job_execute_remove:
 */
static gboolean
li_daemon_job_execute_remove (LiDaemonJob *job)
{
	LiManager *mgr = NULL;
	GError *error = NULL;
	gboolean ret = FALSE;
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	mgr = li_manager_new ();
	g_signal_connect (mgr, "progress",
			G_CALLBACK (li_daemon_job_progress_proxy_cb), priv->mgr_bus);

	li_manager_remove_software (mgr, priv->pkid, &error);
	if (error != NULL) {
		li_daemon_job_emit_error (job, error);
		goto out;
	}

	ret = TRUE;
out:
	if (mgr != NULL)
		g_object_unref (mgr);
	li_daemon_job_emit_finished (job, ret);

	return ret;
}

/**
 * li_daemon_job_execute_install:
 */
static gboolean
li_daemon_job_execute_install (LiDaemonJob *job)
{
	LiInstaller *inst = NULL;
	GError *error = NULL;
	gboolean ret = FALSE;
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	inst = li_installer_new ();
	g_signal_connect (inst, "progress",
				G_CALLBACK (li_daemon_job_progress_proxy_cb), priv->mgr_bus);

	li_installer_open_remote (inst, priv->pkid, &error);
	if (error != NULL) {
		li_daemon_job_emit_error (job, error);
		goto out;
	}

	li_installer_install (inst, &error);
	if (error != NULL) {
		li_daemon_job_emit_error (job, error);
		goto out;
	}

	ret = TRUE;
out:
	if (inst != NULL)
		g_object_unref (inst);
	li_daemon_job_emit_finished (job, ret);

	return ret;
}

/**
 * li_daemon_job_execute_install_local:
 */
static gboolean
li_daemon_job_execute_install_local (LiDaemonJob *job)
{
	LiInstaller *inst = NULL;
	GError *error = NULL;
	gboolean ret = FALSE;
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);


	inst = li_installer_new ();
	g_signal_connect (inst, "progress",
			G_CALLBACK (li_daemon_job_progress_proxy_cb), priv->mgr_bus);

	li_installer_open_file (inst, priv->local_fname, &error);
	if (error != NULL) {
		li_daemon_job_emit_error (job, error);
		goto out;
	}

	li_installer_install (inst, &error);
	if (error != NULL) {
		li_daemon_job_emit_error (job, error);
		goto out;
	}

	ret = TRUE;
out:
	if (inst != NULL)
		g_object_unref (inst);
	li_daemon_job_emit_finished (job, ret);

	return ret;
}

/**
 * li_daemon_job_thread_func:
 **/
static gpointer
li_daemon_job_thread_func (gpointer thread_data)
{
	LiDaemonJob *job = LI_DAEMON_JOB (thread_data);
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	priv->running = TRUE;

	if (priv->kind == LI_JOB_KIND_REFRESH_CACHE) {
		li_daemon_job_execute_refresh_cache (job);
	} else if (priv->kind == LI_JOB_KIND_REMOVE) {
		li_daemon_job_execute_remove (job);
	} else if (priv->kind == LI_JOB_KIND_INSTALL) {
		li_daemon_job_execute_install (job);
	} else if (priv->kind == LI_JOB_KIND_INSTALL_LOCAL) {
		li_daemon_job_execute_install_local (job);
	} else {
		g_warning ("Job with unknown purpose: %i", (int) priv->kind);
	}

	/* unref the thread here as it holds a reference itself and we do
	 * not need to join() this at any stage */
	g_thread_unref (priv->thread);

	priv->running = FALSE;

	return NULL;
}

/**
 * li_daemon_job_run_refresh_cache:
 */
void
li_daemon_job_run_refresh_cache (LiDaemonJob *job)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	priv->kind = LI_JOB_KIND_REFRESH_CACHE;

	/* create & run thread */
	priv->thread = g_thread_new ("LI-DaemonJob",
					  li_daemon_job_thread_func,
					  job);
}

/**
 * li_daemon_job_run_remove_package:
 */
void
li_daemon_job_run_remove_package (LiDaemonJob *job, const gchar *pkid)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	if (priv->pkid != NULL)
		g_free (priv->pkid);
	priv->pkid = g_strdup (pkid);

	priv->kind = LI_JOB_KIND_REMOVE;

	/* create & run thread */
	priv->thread = g_thread_new ("LI-DaemonJob",
					  li_daemon_job_thread_func,
					  job);
}

/**
 * li_daemon_job_run_install:
 */
void
li_daemon_job_run_install (LiDaemonJob *job, const gchar *pkid)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	if (priv->pkid != NULL)
		g_free (priv->pkid);
	priv->pkid = g_strdup (pkid);

	priv->kind = LI_JOB_KIND_INSTALL;

	priv->thread = g_thread_new ("LI-DaemonJob",
					  li_daemon_job_thread_func,
					  job);
}

/**
 * li_daemon_job_run_install_local:
 */
void
li_daemon_job_run_install_local (LiDaemonJob *job, const gchar *fname)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	if (priv->local_fname != NULL)
		g_free (priv->local_fname);
	priv->local_fname = g_strdup (fname);

	priv->kind = LI_JOB_KIND_INSTALL_LOCAL;

	priv->thread = g_thread_new ("LI-DaemonJob",
					  li_daemon_job_thread_func,
					  job);
}

/**
 * li_daemon_job_prepare:
 */
gboolean
li_daemon_job_prepare (LiDaemonJob *job, LiProxyManager *mgr_bus)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);

	/* if we are running, we can't initialize another job */
	if (priv->running)
		return FALSE;

	if (priv->mgr_bus != NULL)
		g_object_unref (priv->mgr_bus);
	priv->mgr_bus = g_object_ref (mgr_bus);

	return TRUE;
}

/**
 * li_daemon_job_is_running:
 */
gboolean
li_daemon_job_is_running (LiDaemonJob *job)
{
	LiDaemonJobPrivate *priv = GET_PRIVATE (job);
	return priv->running;
}

/**
 * li_daemon_job_class_init:
 **/
static void
li_daemon_job_class_init (LiDaemonJobClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_daemon_job_finalize;
}

/**
 * li_daemon_job_new:
 *
 * Creates a new #LiDaemonJob.
 *
 * Returns: (transfer full): a #LiDaemonJob
 *
 **/
LiDaemonJob *
li_daemon_job_new (void)
{
	LiDaemonJob *job;
	job = g_object_new (LI_TYPE_DAEMON_JOB, NULL);
	return LI_DAEMON_JOB (job);
}
