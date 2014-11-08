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
#include <limba.h>

static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;
static gboolean optn_no_fancy = FALSE;

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
 * lipa_install_package:
 */
static gint
lipa_install_package (const gchar *fname)
{
	LiInstaller *inst;
	GError *error = NULL;
	gint res = 0;
	uid_t vuid;
	vuid = getuid ();

	if (vuid != ((uid_t) 0)) {
		li_print_stderr ("This action need superuser permissions.");
		return 2;
	}

	inst = li_installer_new ();
	li_installer_install_package (inst, fname, &error);
	if (error != NULL) {
		li_print_stderr ("Could not install software: %s", error->message);
		g_error_free (error);
		res = 1;
	}
	if (res == 0)
		g_print ("%s\n", _("Software was installed successfully."));

	g_object_unref (inst);
	return res;
}

/**
 * lipa_list_software:
 */
static gint
lipa_list_software (void)
{
	LiManager *mgr;
	GPtrArray *sw;
	guint i;

	mgr = li_manager_new ();

	sw = li_manager_get_installed_software (mgr);
	if (sw == NULL) {
		li_print_stderr ("An error occured while fetching the list of installed software.");
		g_object_unref (mgr);
		return 2;
	}

	for (i = 0; i < sw->len; i++) {
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (sw, i));
		g_print ("[%s] %s %s\n",
				li_pkg_info_get_id (pki),
				li_pkg_info_get_appname (pki),
				li_pkg_info_get_version (pki));
	}

	g_object_unref (mgr);
	return 0;
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

	g_string_append_printf (string, "  %s - %s\n", "install [FILENAME]", _("Install a local software package"));
	g_string_append_printf (string, "  %s - %s\n", "list", _("List installed software"));

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

	command = argv[1];
	if (argc > 2)
		value1 = argv[2];

	if ((g_strcmp0 (command, "install") == 0) || (g_strcmp0 (command, "i") == 0)) {
		exit_code = lipa_install_package (value1);
	} else if (g_strcmp0 (command, "list") == 0) {
		exit_code = lipa_list_software ();
	} else {
		li_print_stderr (_("Command '%s' is unknown."), command);
		exit_code = 1;
		goto out;
	}

out:
	g_option_context_free (opt_context);
	if (options_help != NULL)
		g_free (options_help);

	return exit_code;
}
