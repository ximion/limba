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

#include "config.h"
#include <polkit/polkit.h>

#include "li-dbus-interface.h"
#include "li-daemon-job.h"

typedef struct {
	GMainLoop *loop;

	GDBusObjectManagerServer *obj_manager;
	PolkitAuthority *authority;

	GTimer *timer;
	guint exit_idle_time;
	guint timer_id;
	LiDaemonJob *job;
} LiHelperDaemon;

/**
 * li_daemon_reset_timer:
 **/
static void
li_daemon_reset_timer (LiHelperDaemon *helper)
{
	g_timer_reset (helper->timer);
}

/**
 * li_daemon_init_job:
 */
gboolean
li_daemon_init_job (LiHelperDaemon *helper, LiProxyManager *mgr_bus, GDBusMethodInvocation *context)
{
	gboolean ret;

	/* we don't have a job queue, so we error out in case someone tries to start
	 * multiple jobs at the same time */
	if (li_daemon_job_is_running (helper->job)) {
		g_dbus_method_invocation_return_dbus_error (context, "org.freedesktop.Limba.Manager.Error.Failed",
							"Another job is running at time, Please wait for it to complete.");
		return FALSE;
	}

	ret = li_daemon_job_prepare (helper->job, mgr_bus);
	return ret;
}

/**
 * bus_installer_install_local_cb:
 */
static gboolean
bus_installer_install_local_cb (LiProxyManager *mgr_bus, GDBusMethodInvocation *context, const gchar *fname, LiHelperDaemon *helper)
{
	GError *error = NULL;
	LiInstaller *inst = NULL;
	PolkitAuthorizationResult *pres = NULL;
	PolkitSubject *subject;
	const gchar *sender;

	sender = g_dbus_method_invocation_get_sender (context);

	subject = polkit_system_bus_name_new (sender);
	pres = polkit_authority_check_authorization_sync (helper->authority,
								subject,
								"org.freedesktop.limba.install-package-local",
								NULL,
								POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
								NULL,
								&error);
	g_object_unref (subject);

	if (error != NULL) {
		g_dbus_method_invocation_take_error (context, error);
		goto out;
	}

	if (!polkit_authorization_result_get_is_authorized (pres)) {
		g_dbus_method_invocation_return_dbus_error (context, "org.freedesktop.Limba.Installer.Error.NotAuthorized",
							"Authorization failed.");
		goto out;
	}

	/* initialize our job, in case it is idling */
	if (!li_daemon_init_job (helper, mgr_bus, context))
		goto out;

	/* do the thing */
	li_daemon_job_run_install_local (helper->job, fname);

	li_proxy_manager_complete_install_local (mgr_bus, context);

 out:
	if (inst != NULL)
		g_object_unref (inst);
	if (pres != NULL)
		g_object_unref (pres);

	li_daemon_reset_timer (helper);

	return TRUE;
}

/**
 * bus_installer_install_cb:
 */
static gboolean
bus_installer_install_cb (LiProxyManager *mgr_bus, GDBusMethodInvocation *context, const gchar *pkid, LiHelperDaemon *helper)
{
	GError *error = NULL;
	PolkitAuthorizationResult *pres = NULL;
	PolkitSubject *subject;
	const gchar *sender;

	sender = g_dbus_method_invocation_get_sender (context);

	subject = polkit_system_bus_name_new (sender);
	pres = polkit_authority_check_authorization_sync (helper->authority,
							subject,
							"org.freedesktop.limba.install-package",
							NULL,
							POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
							NULL,
							&error);
	g_object_unref (subject);

	if (error != NULL) {
		g_dbus_method_invocation_take_error (context, error);
		goto out;
	}

	if (!polkit_authorization_result_get_is_authorized (pres)) {
		g_dbus_method_invocation_return_dbus_error (context, "org.freedesktop.Limba.Installer.Error.NotAuthorized",
							"Authorization failed.");
		goto out;
	}

	/* initialize our job, in case it is idling */
	if (!li_daemon_init_job (helper, mgr_bus, context))
		goto out;

	/* do the thing */
	li_daemon_job_run_install (helper->job, pkid);

	li_proxy_manager_complete_install (mgr_bus, context);

out:
	if (pres != NULL)
		g_object_unref (pres);

	li_daemon_reset_timer (helper);

	return TRUE;
}

/**
 * bus_manager_remove_software_cb:
 */
static gboolean
bus_manager_remove_software_cb (LiProxyManager *mgr_bus, GDBusMethodInvocation *context, const gchar *pkid, LiHelperDaemon *helper)
{
	GError *error = NULL;
	PolkitAuthorizationResult *pres = NULL;
	PolkitSubject *subject;
	const gchar *sender;

	sender = g_dbus_method_invocation_get_sender (context);

	subject = polkit_system_bus_name_new (sender);
	pres = polkit_authority_check_authorization_sync (helper->authority,
							subject,
							"org.freedesktop.limba.remove-package",
							NULL,
							POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
							NULL,
							&error);
	g_object_unref (subject);

	if (error != NULL) {
		g_dbus_method_invocation_take_error (context, error);
		goto out;
	}

	if (!polkit_authorization_result_get_is_authorized (pres)) {
		g_dbus_method_invocation_return_dbus_error (context, "org.freedesktop.Limba.Manager.Error.NotAuthorized",
								"Authorization failed.");
		goto out;
	}

	/* initialize our job, in case it is idling */
	if (!li_daemon_init_job (helper, mgr_bus, context))
		goto out;

	/* do the thing */
	li_daemon_job_run_remove_package (helper->job, pkid);

	li_proxy_manager_complete_remove_software (mgr_bus, context);

out:
	if (pres != NULL)
		g_object_unref (pres);

	li_daemon_reset_timer (helper);

	return TRUE;
}

/**
 * on_bus_acquired:
 */
static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, LiHelperDaemon *helper)
{
	LiProxyObjectSkeleton *object;
	LiProxyManager *mgr_bus;
	GError *error = NULL;

	g_print ("Acquired a message bus connection\n");

	helper->authority = polkit_authority_get_sync (NULL, &error);
	g_assert_no_error (error); /* TODO: Meh... Needs smart error handling. */

	helper->obj_manager = g_dbus_object_manager_server_new ("/org/freedesktop/Limba");

	/* create the Manager object */
	object = li_proxy_object_skeleton_new ("/org/freedesktop/Limba/Manager");

	mgr_bus = li_proxy_manager_skeleton_new ();
	li_proxy_object_skeleton_set_manager (object, mgr_bus);
	g_object_unref (mgr_bus);

	g_signal_connect (mgr_bus,
			"handle-remove-software",
			G_CALLBACK (bus_manager_remove_software_cb),
			helper);

	g_signal_connect (mgr_bus,
			"handle-install-local",
			G_CALLBACK (bus_installer_install_local_cb),
			helper);

	g_signal_connect (mgr_bus,
			"handle-install",
			G_CALLBACK (bus_installer_install_cb),
			helper);

	/* export the object */
	g_dbus_object_manager_server_export (helper->obj_manager, G_DBUS_OBJECT_SKELETON (object));
	g_object_unref (object);

	g_dbus_object_manager_server_set_connection (helper->obj_manager, connection);
}

/**
 * on_name_acquired:
 */
static void
on_name_acquired (GDBusConnection *connection, const gchar *name, LiHelperDaemon *helper)
{
	g_print ("Acquired the name %s\n", name);
	li_daemon_reset_timer (helper);
}

/**
 * on_name_lost:
 */
static void
on_name_lost (GDBusConnection *connection, const gchar *name, LiHelperDaemon *helper)
{
	g_print ("Lost the name %s\n", name);

	/* quit, we couldn't own the name */
	g_main_loop_quit (helper->loop);
}

/**
 * li_daemon_timeout_check_cb:
 **/
static gboolean
li_daemon_timeout_check_cb (LiHelperDaemon *helper)
{
	guint idle;

	/* we don't do anything when a job is running */
	if (li_daemon_job_is_running (helper->job)) {
		li_daemon_reset_timer (helper);
		return TRUE;
	}

	idle = (guint) g_timer_elapsed (helper->timer, NULL);
	g_debug ("idle is %i", idle);

	if (idle > helper->exit_idle_time) {
		g_main_loop_quit (helper->loop);
		helper->timer_id = 0;
		return FALSE;
	}

	return TRUE;
}

/* option variables */
static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;

/**
 * main:
 */
gint
main (gint argc, gchar *argv[])
{
	LiHelperDaemon helper;
	guint id;
	GOptionContext *opt_context;
	GError *error = NULL;
	gint res = 0;

	const GOptionEntry daemon_options[] = {
		{ "version", 0, 0, G_OPTION_ARG_NONE, &optn_show_version, "Show the program version", NULL },
		{ "verbose", 0, 0, G_OPTION_ARG_NONE, &optn_verbose_mode, "Display verbose output", NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- Limba daemon");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, daemon_options, NULL);

	g_option_context_parse (opt_context, &argc, &argv, &error);
	if (error != NULL) {
		g_print ("%s\n", error->message);
		g_printerr ("Invalid command line options.");
		g_error_free (error);
		res = 1;
		goto out;
	}

	if (optn_show_version) {
		g_print ("Limba version: %s", VERSION "\n");
		goto out;
	}

	/* just a hack, we might need proper message handling later */
	if (optn_verbose_mode) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}

	/* initialize helper */
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.exit_idle_time = 30;
	helper.timer = g_timer_new ();
	helper.job = li_daemon_job_new ();

	id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
				"org.freedesktop.Limba",
				G_BUS_NAME_OWNER_FLAGS_REPLACE,
				(GBusAcquiredCallback) on_bus_acquired,
				(GBusNameAcquiredCallback) on_name_acquired,
				(GBusNameLostCallback) on_name_lost,
				&helper,
				NULL);

	helper.timer_id = g_timeout_add_seconds (5, (GSourceFunc) li_daemon_timeout_check_cb, &helper);
	g_source_set_name_by_id (helper.timer_id, "[LiDaemon] main poll");

	g_main_loop_run (helper.loop);

	/* cleanup */
	g_bus_unown_name (id);
	g_timer_destroy (helper.timer);
	g_main_loop_unref (helper.loop);
	g_object_unref (helper.job);

	if (helper.timer_id > 0)
		g_source_remove (helper.timer_id);

	if (helper.authority != NULL)
		g_object_unref (helper.authority);

out:
	g_option_context_free (opt_context);

	return res;
}
