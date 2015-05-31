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

#include "config.h"
#include <polkit/polkit.h>

#include "li-dbus-interface.h"
#include "limba.h"

typedef struct {
	GMainLoop *loop;

	GDBusObjectManagerServer *obj_manager;
	PolkitAuthority *authority;

	GTimer *timer;
	guint exit_idle_time;
	guint timer_id;
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
 * on_installer_local_install:
 */
static gboolean
on_installer_local_install (LimbaInstaller *inst_bus, GDBusMethodInvocation *context, const gchar *fname, LiHelperDaemon *helper)
{
	GError *error = NULL;
	LiInstaller *inst = NULL;
	PolkitAuthorizationResult *pres = NULL;
	PolkitSubject *subject;
	const gchar *sender;

	li_daemon_reset_timer (helper);

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

	if (fname == NULL) {
		g_dbus_method_invocation_return_dbus_error (context, "org.freedesktop.Limba.Installer.Error.Failed",
													"The filename must not be NULL.");
		goto out;
	}

	if (!g_str_has_prefix (fname, "/")) {
		g_dbus_method_invocation_return_dbus_error (context, "org.freedesktop.Limba.Installer.Error.Failed",
													"The path to the IPK package to install must be absolute.");
		goto out;
	}

	inst = li_installer_new ();

	li_installer_open_file (inst, fname, &error);
	if (error != NULL) {
		g_dbus_method_invocation_take_error (context, error);
		goto out;
	}

	li_installer_install (inst, &error);
	if (error != NULL) {
		g_dbus_method_invocation_take_error (context, error);
		goto out;
	}

	limba_installer_complete_local_install (inst_bus, context);

 out:
	if (inst != NULL)
		g_object_unref (inst);
	if (pres != NULL)
		g_object_unref (pres);

	return TRUE;
}

/**
 * on_installer_install:
 */
static gboolean
on_installer_install (LimbaInstaller *inst_bus, GDBusMethodInvocation *context, const gchar *pkid, LiHelperDaemon *helper)
{
	GError *error = NULL;
	LiInstaller *inst = NULL;
	PolkitAuthorizationResult *pres = NULL;
	PolkitSubject *subject;
	const gchar *sender;

	li_daemon_reset_timer (helper);

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

	if (pkid == NULL) {
		g_dbus_method_invocation_return_dbus_error (context, "org.freedesktop.Limba.Installer.Error.Failed",
													"The bundle identifier was NULL.");
		goto out;
	}

	inst = li_installer_new ();

	li_installer_open_remote (inst, pkid, &error);
	if (error != NULL) {
		g_dbus_method_invocation_take_error (context, error);
		goto out;
	}

	li_installer_install (inst, &error);
	if (error != NULL) {
		g_dbus_method_invocation_take_error (context, error);
		goto out;
	}

	limba_installer_complete_install (inst_bus, context);

 out:
	if (inst != NULL)
		g_object_unref (inst);
	if (pres != NULL)
		g_object_unref (pres);

	return TRUE;
}

/**
 * on_bus_acquired:
 */
static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, LiHelperDaemon *helper)
{
	LimbaObjectSkeleton *object;
	LimbaInstaller *inst_bus;
	LimbaManager *mgr_bus;
	GError *error = NULL;

	g_print ("Acquired a message bus connection\n");

	helper->authority = polkit_authority_get_sync (NULL, &error);
	g_assert_no_error (error); /* TODO: Meh... Needs smart error handling. */

	helper->obj_manager = g_dbus_object_manager_server_new ("/org/freedesktop/Limba");

	/* create the Installer object */
	object = limba_object_skeleton_new ("/org/freedesktop/Limba/Installer");

	inst_bus = limba_installer_skeleton_new ();
	limba_object_skeleton_set_installer (object, inst_bus);
	g_object_unref (inst_bus);

	g_signal_connect (inst_bus,
					"handle-local-install",
					G_CALLBACK (on_installer_local_install),
					helper);

	g_signal_connect (inst_bus,
					"handle-install",
					G_CALLBACK (on_installer_install),
					helper);

	/* export the object */
	g_dbus_object_manager_server_export (helper->obj_manager, G_DBUS_OBJECT_SKELETON (object));
	g_object_unref (object);

	/* create the Manager object */
	object = limba_object_skeleton_new ("/org/freedesktop/Limba/Manager");

	mgr_bus = limba_manager_skeleton_new ();
	limba_object_skeleton_set_manager (object, mgr_bus);
	g_object_unref (mgr_bus);

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

	helper.loop = g_main_loop_new (NULL, FALSE);

	helper.exit_idle_time = 20;
	helper.timer = g_timer_new ();

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

	if (helper.timer_id > 0)
		g_source_remove (helper.timer_id);

	if (helper.authority != NULL)
		g_object_unref (helper.authority);

out:
	g_option_context_free (opt_context);

	return res;
}
