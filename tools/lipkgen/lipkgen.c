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

#include <stdio.h>
#include <locale.h>
#include <glib/gi18n-lib.h>
#include <limba.h>
#include <appstream.h>

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
	gint res = 0;
	GError *error = NULL;
	LiPkgBuilder *builder;

	if (dir == NULL) {
		li_print_stderr (_("You need to specify a directory with build-metadata."));
		return 1;
	}

	builder = li_pkg_builder_new ();
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
 * li_get_stdin:
 */
static gchar*
li_get_stdin (void)
{
	gchar *tmp = NULL;
	gchar *str = NULL;
	size_t size = 0;

	if (getline(&tmp, &size, stdin) > 0) {
		g_strstrip (tmp);
		if (g_strcmp0 (tmp, "") != 0)
			str = g_strdup (tmp);
	}
	g_free (tmp);

	return str;
}

/**
 * pkgen_make_template:
 */
static gint
pkgen_make_template (const gchar *dir)
{
	gint res = 0;
	gchar *res_dir = NULL;
	gchar *tmp;
	gchar *fname;
	gboolean appstream_linked = FALSE;
	GError *error = NULL;

	if (dir == NULL) {
		tmp = g_get_current_dir ();
		res_dir = g_build_filename (tmp, "pkginstall", NULL);
		g_free (tmp);
	} else {
		res_dir = g_strdup (dir);
	}

	g_mkdir_with_parents (res_dir, 0755);

	g_print (_("Do you have an AppStream XML file for your software? [y/N]"));
	tmp = li_get_stdin ();
	if (tmp != NULL) {
		gchar *str;
		str = g_utf8_strdown (tmp, -1);
		g_free (tmp);
		g_strstrip (str);

		if (g_strcmp0 (str, "y") == 0) {
			g_print ("%s ", _("Please specify a path to the AppStream XML data:"));
			tmp = li_get_stdin ();
			if (tmp != NULL) {
				gchar *asfile;
				asfile = g_build_filename (res_dir, "metainfo.xml", NULL);
				/* TODO: create relative symlink */
				symlink (tmp, asfile);
				g_free (tmp);
				appstream_linked = TRUE;
			} else {
				res = 1;
				g_print ("%s\n", _("No path given. Exiting."));
				g_free (str);
				goto out;
			}
		}
		g_free (str);
    }

    if (!appstream_linked) {
		AsComponent *cpt;
		gchar *asfile;
		cpt = as_component_new ();

		while (TRUE) {
			g_print ("%s ", _("Your software needs a unique name.\nIn case of a GUI application, this is its .desktop filename.\nUnique software name:"));
			tmp = li_get_stdin ();
			if (tmp != NULL) {
				as_component_set_id (cpt, tmp);
				g_free (tmp);
				break;
			}
		}

		while (TRUE) {
			g_print ("%s ", _("Define a software name (human readable):"));
			tmp = li_get_stdin ();
			if (tmp != NULL) {
				as_component_set_name (cpt, tmp);
				g_free (tmp);
				break;
			}
		}

		while (TRUE) {
			g_print ("%s ", _("Define a software version:"));
			tmp = li_get_stdin ();
			if (tmp != NULL) {
				AsRelease *rel;
				rel = as_release_new ();
				as_release_set_version (rel, tmp);
				as_component_add_release (cpt, rel);
				g_object_unref (rel);
				g_free (tmp);
				break;
			}
		}

		while (TRUE) {
			g_print ("%s ", _("Write a short summary (one sentence) about your software:"));
			tmp = li_get_stdin ();
			if (tmp != NULL) {
				as_component_set_summary (cpt, tmp);
				g_free (tmp);
				break;
			}
		}

		asfile = g_build_filename (res_dir, "metainfo.xml", NULL);
		tmp = as_component_to_xml (cpt);
		g_file_set_contents (asfile, tmp, -1, &error);
		g_free (tmp);
		if (error != NULL) {
			li_print_stderr (_("Unable to write AppStream data. %s"), error->message);
			g_error_free (error);
			res = 2;
			g_free (asfile);
			goto out;
		}

		if (g_file_test ("/usr/bin/xmllint", G_FILE_TEST_EXISTS)) {
			gchar *cmd;
			/* we can try to pretty-print our XML file. Using xmllint from the environment should
			 * not be a security risk here */
			cmd = g_strdup_printf ("xmllint --format %s -o %s", asfile, asfile);

			/* any error / exit-code isn't relevant here - a failed attempt to format the XML will do no harm */
			g_spawn_command_line_sync (cmd, NULL, NULL, NULL, NULL);
			g_free (cmd);
		}
		g_free (asfile);
    }

    fname = g_build_filename (res_dir, "control", NULL);
	g_file_set_contents (fname, "Format-Version: 1.0\n\nRequires:\n", -1, &error);
	g_free (fname);
	if (error != NULL) {
		li_print_stderr (_("Unable to write 'control' file. %s"), error->message);
		g_error_free (error);
		res = 2;
		goto out;
	}

	li_print_stdout ("\n========");\
	li_print_stdout (_("Created project template in '%s'.\n\n" \
"Please edit the files in that directory, e.g. add a long description to your\n" \
"application and specify its run-time dependencies.\n" \
"When you are done with this, build your software with --prefix=%s\n" \
"and install it into the inst_target subdirectory of your 'pkginstall' directory.\n" \
"Then run 'lipkgen build pkginstall/' to create your package. \n" \
"If you want to embed dependencies, place their IPK packages in the 'repo/'\n" \
"subdirectory of 'pkginstall/'"), res_dir, LI_SW_ROOT_PREFIX);
	li_print_stdout ("========\n");

out:
	g_free (res_dir);
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
				/* these are commands we can use with lipa */
				_("Subcommands:"));

	g_string_append_printf (string, "  %s - %s\n", "build [DIRECTORY] [PKGNAME]", _("Create a new package using data found in DIRECTORY."));
	g_string_append_printf (string, "  %s - %s\n", "make-template", _("Create sources for a new package."));

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
	} else if (g_strcmp0 (command, "make-template") == 0) {
		exit_code = pkgen_make_template (value1);
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
