/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Matthias Klumpp <matthias@tenstral.net>
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

#include <stdio.h>
#include <locale.h>
#include <glib/gi18n-lib.h>
#include <limba.h>

static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;
static gboolean optn_no_fancy = FALSE;
static gboolean optn_no_signature = FALSE;

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
	gint res = 0;
	GError *error = NULL;
	LiPkgBuilder *builder;

	if (dir == NULL) {
		li_print_stderr (_("You need to specify a directory with build-metadata."));
		return 1;
	}

	builder = li_pkg_builder_new ();
	li_pkg_builder_set_sign_package (builder, TRUE);
	if (optn_no_signature)
		li_pkg_builder_set_sign_package (builder, FALSE);

	li_pkg_builder_create_package_from_dir (builder, dir, out_fname, &error);
	if (error != NULL) {
		li_print_stderr ("Failed to create package: %s", error->message);
		g_error_free (error);
		res = 1;
	}
	g_object_unref (builder);

	return res;
}

/**
 * pkgen_unpack_pkg:
 */
static gint
pkgen_unpack_pkg (const gchar *fname, const gchar *dir)
{
	LiPackage *pkg = NULL;
	gint res = 1;
	gchar *dest_dir;
	GError *error = NULL;

	if (fname == NULL) {
		li_print_stderr (_("No package file given for extraction."));
		goto out;
	}

	pkg = li_package_new ();
	li_package_open_file (pkg, fname, &error);
	if (error != NULL) {
		li_print_stderr (_("Unable to open package. %s"), error->message);
		g_error_free (error);
		goto out;
	}

	if (dir != NULL)
		dest_dir = g_strdup (dir);
	else
		dest_dir = g_get_current_dir ();

	li_package_extract_contents (pkg, dest_dir, &error);
	if (error != NULL) {
		li_print_stderr (_("Unable to unpack package. %s"), error->message);
		g_error_free (error);
		goto out;
	}

	res = 0;
out:
	if (pkg != NULL)
		g_object_unref (pkg);

	return res;
}

/**
 * pkgen_get_summary:
 **/
static gchar *
pkgen_get_summary ()
{
	GString *string;
	string = g_string_new ("");

	/* TRANSLATORS: This is the header to the --help menu */
	g_string_append_printf (string, "%s\n\n%s\n", _("Limba package builder"),
				/* these are commands we can use with lipkgen */
				_("Subcommands:"));

	g_string_append_printf (string, "  %s - %s\n", "build [DIRECTORY] [PKGNAME]", _("Create a new package using data found in DIRECTORY."));
	g_string_append_printf (string, "  %s - %s\n", "unpack-pkg [PKGNAME] [DIRECTORY]", _("Unpack the Limba package to a directory."));

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
		{ "no-signature", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_no_signature, _("Do not sign the package"), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- Limba package builder");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, client_options, NULL);

	/* set the summary text */
	summary = pkgen_get_summary ();
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
	} else if (g_strcmp0 (command, "unpack-pkg") == 0) {
		exit_code = pkgen_unpack_pkg (value1, value2);
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
