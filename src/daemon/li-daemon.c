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

#include <polkit/polkit.h>

#include "li-dbus-interface.h"
#include "limba.h"

/* ---------------------------------------------------------------------------------------------------- */

static GDBusObjectManagerServer *obj_manager = NULL;
static PolkitAuthority *authority = NULL;

/**
 * on_installer_local_install:
 */
static gboolean
on_installer_local_install (LimbaInstaller *inst_bus, GDBusMethodInvocation *context, const gchar *fname, gpointer user_data)
{
	GError *error = NULL;
	LiInstaller *inst = NULL;
	PolkitAuthorizationResult *pres = NULL;
	PolkitSubject *subject;
	const gchar *sender;

	sender = g_dbus_method_invocation_get_sender (context);

	subject = polkit_system_bus_name_new (sender);
	pres = polkit_authority_check_authorization_sync (authority,
													subject,
													"org.test.limba.install-package-local",
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
		g_dbus_method_invocation_return_dbus_error (context, "org.test.Limba.Installer.Error.NotAuthorized",
													"Authorization failed.");
		goto out;
	}

	if (fname == NULL) {
		g_dbus_method_invocation_return_dbus_error (context, "org.test.Limba.Installer.Error.Failed",
													"The filename must not be NULL.");
		goto out;
	}

	if (!g_str_has_prefix (fname, "/")) {
		g_dbus_method_invocation_return_dbus_error (context, "org.test.Limba.Installer.Error.Failed",
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
 * on_bus_acquired:
 */
static void
on_bus_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	LimbaObjectSkeleton *object;
	LimbaInstaller *inst_bus;
	LimbaManager *mgr_bus;
	GError *error = NULL;

	g_print ("Acquired a message bus connection\n");

	authority = polkit_authority_get_sync (NULL, &error);
	g_assert_no_error (error); /* TODO: Meh... Needs smart error handling. */

	obj_manager = g_dbus_object_manager_server_new ("/org/test/Limba");

	/* create the Installer object */
	object = limba_object_skeleton_new ("/org/test/Limba/Installer");

	inst_bus = limba_installer_skeleton_new ();
	limba_object_skeleton_set_installer (object, inst_bus);
	g_object_unref (inst_bus);

	g_signal_connect (inst_bus,
					"handle-local-install",
					G_CALLBACK (on_installer_local_install),
					NULL);

	/* export the object */
	g_dbus_object_manager_server_export (obj_manager, G_DBUS_OBJECT_SKELETON (object));
	g_object_unref (object);

	/* create the Manager object */
	object = limba_object_skeleton_new ("/org/test/Limba/Manager");

	mgr_bus = limba_manager_skeleton_new ();
	limba_object_skeleton_set_manager (object, mgr_bus);
	g_object_unref (mgr_bus);

	/* export the object */
	g_dbus_object_manager_server_export (obj_manager, G_DBUS_OBJECT_SKELETON (object));
	g_object_unref (object);

	g_dbus_object_manager_server_set_connection (obj_manager, connection);
}

/**
 * on_name_acquired:
 */
static void
on_name_acquired (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	g_print ("Acquired the name %s\n", name);
}

/**
 * on_name_lost:
 */
static void
on_name_lost (GDBusConnection *connection, const gchar *name, gpointer user_data)
{
	g_print ("Lost the name %s\n", name);
}

/**
 * main:
 */
gint
main (gint argc, gchar *argv[])
{
	GMainLoop *loop;
	guint id;

	loop = g_main_loop_new (NULL, FALSE);

	id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
						"org.test.Limba",
						G_BUS_NAME_OWNER_FLAGS_REPLACE,
						on_bus_acquired,
						on_name_acquired,
						on_name_lost,
						loop,
						NULL);

	g_main_loop_run (loop);

	g_bus_unown_name (id);
	g_main_loop_unref (loop);

	if (authority != NULL)
		g_object_unref (authority);

	return 0;
}
