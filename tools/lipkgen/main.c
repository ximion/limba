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

#include "li-pkg-builder.h"

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
 * pkgen_build_package:
 */
static gint
pkgen_build_package (const gchar *dir, const gchar *out_fname)
{
	if (dir == NULL) {
		li_print_stderr (_("You need to specify a directory with build-metadata."));
		return 1;
	}

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
	g_string_append_printf (string, "%s\n\n%s\n", _("Limba package builder"),
				/* these are commands we can use with lipa */
				_("Subcommands:"));

	g_string_append_printf (string, "  %s - %s\n", "build [DIRECTORY] [PKGNAME]", _("Create a new package from using data found in DIRECTORY"));

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
	gchar *value2 = NULL;
	gchar *summary;
	gchar *options_help = NULL;

	const GOptionEntry client_options[] = {
		{ "version", 0, 0, G_OPTION_ARG_NONE, &optn_show_version, _("Show the program version"), NULL },
		{ "verbose", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_verbose_mode, _("Show extra debugging information"), NULL },
		{ "no-fancy", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_no_fancy, _("Don't show \"fancy\" output"), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- Limba package builder");
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
	if (argc > 3)
		value2 = argv[3];

	if ((g_strcmp0 (command, "build") == 0) || (g_strcmp0 (command, "b") == 0)) {
		exit_code = pkgen_build_package (value1, value2);
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
