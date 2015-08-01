/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <config.h>
#include <locale.h>
#include <glib/gi18n-lib.h>
#include <signal.h>
#include <stdlib.h>
#include <limba.h>

#include "li-console-utils.h"

static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;
static gboolean optn_no_fancy = FALSE;
static gboolean optn_no_fail = FALSE;

/**
 * li_print_stderr:
 */
static void
li_print_stderr (const gchar *format, ...)
{
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_printerr ("%s\n", str);

	g_free (str);
}

/**
 * li_print_stdout:
 */
static void
li_print_stdout (const gchar *format, ...)
{
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	g_print ("%s\n", str);

	g_free (str);
}

/**
 * lipa_check_su:
 */
gboolean
lipa_check_su (void)
{
	uid_t vuid;
	vuid = getuid ();

	if (vuid != ((uid_t) 0)) {
		li_print_stderr ("This action needs superuser permissions.");
		return FALSE;
	}

	return TRUE;
}

/**
 * lipa_list_software:
 */
static gint
lipa_list_software (void)
{
	LiManager *mgr;
	GList *sw = NULL;
	GList *l;
	gint exit_code = 0;
	GError *error = NULL;

	mgr = li_manager_new ();

	sw = li_manager_get_software_list (mgr, &error);
	if (error != NULL) {
		li_print_stderr ("An error occured while fetching the software-list: %s", error->message);
		exit_code = 2;
		goto out;
	}

	for (l = sw; l != NULL; l = l->next) {
		gchar *state;
		LiPkgInfo *pki = LI_PKG_INFO (l->data);

		if (li_pkg_info_has_flag (pki, LI_PACKAGE_FLAG_INSTALLED))
			state = g_strdup ("i");
		else if (li_pkg_info_has_flag (pki, LI_PACKAGE_FLAG_AVAILABLE))
			state = g_strdup ("a");
		else
			state = g_strdup ("?");

		g_print ("[%s]...%s:\t\t%s %s\n",
				state,
				li_pkg_info_get_id (pki),
				li_pkg_info_get_appname (pki),
				li_pkg_info_get_version (pki));
		g_free (state);
	}

out:
	g_list_free (sw);
	g_object_unref (mgr);
	if (error != NULL)
		g_error_free (error);

	return exit_code;
}

/**
 * li_installer_progress_cb:
 */
static void
li_installer_progress_cb (LiInstaller *inst, guint percentage, const gchar *id, gpointer user_data)
{
	/* only catch the main progress */
	if (id != NULL)
		return;

	li_draw_progress_bar (_("Progress"), percentage);
}

/**
 * li_installer_stage_changed_cb:
 */
static void
li_installer_stage_changed_cb (LiInstaller *inst, LiPackageStage stage, const gchar *id, gpointer user_data)
{
	/* we ignore global notifications */
	if (id == NULL)
		return;

	switch (stage) {
		case LI_PACKAGE_STAGE_DOWNLOADING:
			li_write_progress_step (_("Downloading %s"), id);
			return;
		case LI_PACKAGE_STAGE_VERIFYING:
			li_write_progress_step (_("Verifying %s"), id);
			return;
		case LI_PACKAGE_STAGE_INSTALLING:
			li_write_progress_step (_("Installing %s"), id);
			return;
		case LI_PACKAGE_STAGE_FINISHED:
			li_write_progress_step (_("Completed %s"), id);
			return;
		default:
			return;
	}


}

/**
 * lipa_install_package:
 */
static gint
lipa_install_package (const gchar *pkgid)
{
	LiInstaller *inst = NULL;
	GError *error = NULL;
	gint res = 0;

	if (pkgid == NULL) {
		li_print_stderr (_("You need to specify a package to install."));
		res = 4;
		goto out;
	}

	if (!lipa_check_su ())
		return 2;

	inst = li_installer_new ();
	g_signal_connect (inst, "progress",
						G_CALLBACK (li_installer_progress_cb), NULL);
	g_signal_connect (inst, "stage-changed",
						G_CALLBACK (li_installer_stage_changed_cb), NULL);

	li_installer_open_remote (inst, pkgid, &error);
	if (error != NULL) {
		li_abort_progress_bar ();
		li_print_stderr (_("Could not find package: %s"), error->message);
		g_error_free (error);
		res = 1;
		goto out;
	}

	li_installer_install (inst, &error);
	if (error != NULL) {
		li_abort_progress_bar ();
		li_print_stderr (_("Could not install software: %s"), error->message);
		g_error_free (error);
		res = 1;
	}

out:
	if (res == 0)
		g_print ("%s\n", _("Software was installed successfully."));

	g_object_unref (inst);
	return res;
}

/**
 * lipa_install_local_package:
 */
static gint
lipa_install_local_package (const gchar *fname)
{
	LiInstaller *inst = NULL;
	GError *error = NULL;
	gint res = 0;

	if (fname == NULL) {
		li_print_stderr (_("You need to specify a package to install."));
		res = 4;
		goto out;
	}

	if (!lipa_check_su ())
		return 2;

	inst = li_installer_new ();
	g_signal_connect (inst, "progress",
						G_CALLBACK (li_installer_progress_cb), NULL);

	li_installer_open_file (inst, fname, &error);
	if (error != NULL) {
		li_print_stderr (_("Could not open package: %s"), error->message);
		g_error_free (error);
		res = 1;
		goto out;
	}

	li_installer_install (inst, &error);
	if (error != NULL) {
		li_print_stderr (_("Could not install software: %s"), error->message);
		g_error_free (error);
		res = 1;
	}

out:
	if (res == 0)
		g_print ("%s\n", _("Software was installed successfully."));

	g_object_unref (inst);
	return res;
}

/**
 * lipa_remove_software:
 */
static gint
lipa_remove_software (const gchar *pkgid)
{
	LiManager *mgr;
	gint res = 0;
	GError *error = NULL;

	if (!lipa_check_su ())
		return 2;

	if (pkgid == NULL) {
		li_print_stderr (_("You need to specify a package to remove."));
		return 4;
	}

	mgr = li_manager_new ();

	li_manager_remove_software (mgr, pkgid, &error);
	if (error != NULL) {
		li_print_stderr ("Could not remove software: %s", error->message);
		g_error_free (error);
		res = 1;
	}

	g_object_unref (mgr);
	return res;
}

/**
 * lipa_cleanup:
 */
static gint
lipa_cleanup (void)
{
	LiManager *mgr;
	gint res = 0;
	GError *error = NULL;

	if (!lipa_check_su ())
		return 2;

	mgr = li_manager_new ();

	li_manager_cleanup (mgr, &error);
	if (error != NULL) {
		li_print_stderr ("Could not cleanup cruft: %s", error->message);
		g_error_free (error);
		res = 1;
	}
	g_object_unref (mgr);

	return res;
}

/**
 * lipa_refresh:
 */
static gint
lipa_refresh (void)
{
	LiManager *mgr;
	gint res = 0;
	GError *error = NULL;

	if (!lipa_check_su ())
		return 2;

	mgr = li_manager_new ();

	li_manager_refresh_cache (mgr, &error);
	if (error != NULL) {
		li_print_stderr ("Could not refresh cache: %s", error->message);
		g_error_free (error);
		res = 1;
	}
	g_object_unref (mgr);

	return res;
}

/**
 * lipa_trust_key:
 */
static gint
lipa_trust_key (const gchar *fpr)
{
	LiManager *mgr;
	gint res = 0;
	GError *error = NULL;

	if (!lipa_check_su ())
		return 2;

	if (fpr == NULL) {
		li_print_stderr (_("You need to specify a key fingerprint."));
		return 4;
	}

	mgr = li_manager_new ();

	li_manager_receive_key (mgr, fpr, &error);
	if (error != NULL) {
		li_print_stderr ("Could not add key: %s", error->message);
		g_error_free (error);
		res = 1;
	}

	g_object_unref (mgr);
	return res;
}

/**
 * lipa_list_updates:
 */
static gint
lipa_list_updates (void)
{
	LiManager *mgr;
	GList *updates = NULL;
	GList *l;
	gint exit_code = 0;
	GError *error = NULL;

	mgr = li_manager_new ();

	updates = li_manager_get_update_list (mgr, &error);
	if (error != NULL) {
		li_print_stderr ("An error occured while fetching the software-list: %s", error->message);
		exit_code = 2;
		goto out;
	}

	for (l = updates; l != NULL; l = l->next) {
		LiPkgInfo *old_p;
		LiPkgInfo *new_p;
		LiUpdateItem *uitem = LI_UPDATE_ITEM (l->data);

		old_p = li_update_item_get_installed_pkg (uitem);
		new_p = li_update_item_get_available_pkg (uitem);

		g_print ("%s: (%s) -> (%s)\n",
			li_pkg_info_get_name (old_p),
			li_pkg_info_get_version (old_p),
			li_pkg_info_get_version (new_p));
	}

out:
	g_list_free (updates);
	g_object_unref (mgr);
	if (error != NULL)
		g_error_free (error);

	return exit_code;
}

/**
 * lipa_update:
 */
static gint
lipa_update (void)
{
	LiManager *mgr;
	gint exit_code = 0;
	GError *error = NULL;

	if (!lipa_check_su ())
		return 2;

	mgr = li_manager_new ();

	li_manager_apply_updates (mgr, &error);
	if (error != NULL) {
		li_print_stderr ("Could not apply updates: %s", error->message);
		exit_code = 2;
		goto out;
	}

out:
	g_object_unref (mgr);
	if (error != NULL)
		g_error_free (error);

	return exit_code;
}

/*
 * sigint_handler_cb:
 *
 * Handle SIGINT
 */
void
sigint_handler_cb (int sig)
{
	/* don't break the progress bar if we suddenly abort */
	li_abort_progress_bar ();

	/* quit */
	exit (130);
}

/**
 * lipa_get_summary:
 **/
static gchar *
lipa_get_summary ()
{
	GString *string;
	string = g_string_new ("");

	/* TRANSLATORS: This is the header to the --help menu */
	g_string_append_printf (string, "%s\n\n%s\n", _("Limba software manager"),
				/* these are commands we can use with lipa */
				_("Subcommands:"));

	g_string_append_printf (string, "  %s - %s\n", "list", _("List installed software"));
	g_string_append_printf (string, "  %s - %s\n", "install [PKGID]", _("Install software from a repository"));
	g_string_append_printf (string, "  %s - %s\n", "install-local [FILENAME]", _("Install a local software package"));
	g_string_append_printf (string, "  %s - %s\n", "remove  [PKGID]", _("Remove an installed software package"));
	g_string_append_printf (string, "  %s - %s\n", "refresh", _("Refresh the cache of available packages"));
	g_string_append_printf (string, "  %s - %s\n", "cleanup", _("Cleanup cruft packages"));
	g_string_append (string, "\n");

	g_string_append_printf (string, "  %s - %s\n", "trust-key [FPR]", _("Add a PGP key to the trusted database."));

	return g_string_free (string, FALSE);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GOptionContext *opt_context;
	GError *error = NULL;

	int exit_code = 0;
	gchar *command = NULL;
	gchar *value1 = NULL;
	gchar *summary;
	gchar *options_help = NULL;

	const GOptionEntry client_options[] = {
		{ "version", 0, 0, G_OPTION_ARG_NONE, &optn_show_version, _("Show the program version"), NULL },
		{ "verbose", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_verbose_mode, _("Show extra debugging information"), NULL },
		{ "no-fancy", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_no_fancy, _("Don't show \"fancy\" output"), NULL },
		{ "no-fail", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_no_fail, _("Do not fail action with an error code."), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- Limba software manager");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, client_options, NULL);

	/* set the summary text */
	summary = lipa_get_summary ();
	g_option_context_set_summary (opt_context, summary) ;
	options_help = g_option_context_get_help (opt_context, TRUE, NULL);
	g_free (summary);

	g_option_context_parse (opt_context, &argc, &argv, &error);
	if (error != NULL) {
		gchar *msg;
		msg = g_strconcat (error->message, "\n", NULL);
		g_print ("%s\n", msg);
		g_free (msg);
		li_print_stderr (_("Run '%s --help' to see a full list of available command line options."), argv[0]);
		exit_code = 1;
		g_error_free (error);
		goto out;
	}

	if (optn_show_version) {
		li_print_stdout (_("Limba version: %s"), VERSION);
		goto out;
	}

	/* just a hack, we might need proper message handling later */
	if (optn_verbose_mode) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}

	if (argc < 2) {
		g_printerr ("%s\n", _("You need to specify a command."));
		li_print_stderr (_("Run '%s --help' to see a full list of available command line options."), argv[0]);
		exit_code = 1;
		goto out;
	}

	/* handle CTRL+C */
	signal (SIGINT, sigint_handler_cb);

	command = argv[1];
	if (argc > 2)
		value1 = argv[2];

	if (g_strcmp0 (command, "list") == 0) {
		exit_code = lipa_list_software ();
	} else if ((g_strcmp0 (command, "install") == 0) || (g_strcmp0 (command, "i") == 0)) {
		exit_code = lipa_install_package (value1);
	} else if (g_strcmp0 (command, "install-local") == 0) {
		exit_code = lipa_install_local_package (value1);
	} else if ((g_strcmp0 (command, "remove") == 0) || (g_strcmp0 (command, "r") == 0)) {
		exit_code = lipa_remove_software (value1);
	} else if (g_strcmp0 (command, "refresh") == 0) {
		exit_code = lipa_refresh ();
	} else if (g_strcmp0 (command, "cleanup") == 0) {
		exit_code = lipa_cleanup ();
	} else if (g_strcmp0 (command, "trust-key") == 0) {
		exit_code = lipa_trust_key (value1);
	} else if (g_strcmp0 (command, "list-updates") == 0) {
		exit_code = lipa_list_updates ();
	} else if (g_strcmp0 (command, "update") == 0) {
		exit_code = lipa_update ();
	} else {
		li_print_stderr (_("Command '%s' is unknown."), command);
		exit_code = 1;
		goto out;
	}

out:
	g_option_context_free (opt_context);
	if (options_help != NULL)
		g_free (options_help);

	/* we should not emit an error code */
	if (optn_no_fail)
		return 0;

	return exit_code;
}
