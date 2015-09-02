/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Matthias Klumpp <matthias@tenstral.net>
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

#include "li-console-utils.h"
#include "li-build-master.h"

static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;
static gboolean optn_no_fancy = FALSE;

/**
 * bcli_repo_init:
 */
static gint
bcli_repo_init (const gchar *repodir)
{
	gint res = 0;
	gchar *rdir;
	GError *error = NULL;
	LiRepository *repo;

	if (repodir == NULL)
		rdir = g_get_current_dir ();
	else
		rdir = g_strdup (repodir);

	repo = li_repository_new ();

	li_repository_open (repo, rdir, &error);
	if (error != NULL) {
		li_print_stderr (_("Failed to initialize repository: %s"), error->message);
		res = 1;
		goto out;
	}

	li_repository_save (repo, &error);
	if (error != NULL) {
		li_print_stderr (_("Failed to initialize repository: %s"), error->message);
		res = 1;
		goto out;
	}

out:
	g_free (rdir);
	g_object_unref (repo);
	if (error != NULL)
		g_error_free (error);

	return res;
}

/**
 * bcli_repo_add_package:
 */
static gint
bcli_repo_add_package (const gchar *fname, const gchar *repodir)
{
	gint res = 0;
	gchar *rdir;
	GError *error = NULL;
	LiRepository *repo = NULL;

	if (repodir == NULL)
		rdir = g_get_current_dir ();
	else
		rdir = g_strdup (repodir);

	if (fname == NULL) {
		li_print_stderr (_("You need to specify a package file to add to the repository."));
		res = 2;
		goto out;
	}

	repo = li_repository_new ();

	li_repository_open (repo, rdir, &error);
	if (error != NULL) {
		li_print_stderr (_("Failed to open repository: %s"), error->message);
		res = 1;
		goto out;
	}

	li_repository_add_package (repo, fname, &error);
	if (error != NULL) {
		li_print_stderr (_("Failed to add package: %s"), error->message);
		res = 1;
		goto out;
	}

	li_repository_save (repo, &error);
	if (error != NULL) {
		li_print_stderr (_("Failed to save repository indices: %s"), error->message);
		res = 1;
		goto out;
	}

	li_repository_create_icon_tarballs (repo, &error);
	if (error != NULL) {
		li_print_stderr (_("Failed to update icon tarball: %s"), error->message);
		res = 1;
		goto out;
	}

out:
	g_free (rdir);
	g_object_unref (repo);
	if (error != NULL)
		g_error_free (error);

	return res;
}

/**
 * bcli_execute_build:
 */
static gint
bcli_execute_build (const gchar *srcdir)
{
	LiBuildMaster *bmaster;
	gint res = 0;
	gchar *sdir;
	GError *error = NULL;

	if (srcdir == NULL)
		sdir = g_get_current_dir ();
	else
		sdir = g_strdup (srcdir);

	bmaster = li_build_master_new ();

	li_build_master_init_build (bmaster, sdir, &error);
	if (error != NULL) {
		li_print_stdout ("-- Error --");
		li_print_stderr ("%s", error->message);
		res = 1;
		goto out;
	}

	test_print_script (bmaster);

out:
	g_free (sdir);
	g_object_unref (bmaster);

	return res;
}

/**
 * libuild_get_summary:
 **/
static gchar *
libuild_get_summary ()
{
	GString *string;
	string = g_string_new ("");

	/* TRANSLATORS: This is the header to the --help menu */
	g_string_append_printf (string, "%s\n\n%s\n", _("Limba build tool"),
				/* these are commands we can use with limba-build */
				_("Subcommands:"));

	g_string_append_printf (string, "  %s - %s\n", "run [DIRECTORY]", _("Build the software following its build recipe"));
	g_string_append_printf (string, "  %s - %s\n", "repo-init [DIRECTORY]", _("Initialize a new repository in DIRECTORY."));
	g_string_append_printf (string, "  %s - %s\n", "repo-add [PKGNAME] [DIRECTORY]", _("Add a package to the repository"));

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
		{ "verbose", 0, 0, G_OPTION_ARG_NONE, &optn_verbose_mode, _("Show extra debugging information"), NULL },
		{ "no-fancy", 0, 0, G_OPTION_ARG_NONE, &optn_no_fancy, _("Don't show \"fancy\" output"), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- Limba build tool");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, client_options, NULL);

	/* set the summary text */
	summary = libuild_get_summary ();
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

	if (g_strcmp0 (command, "repo-init") == 0) {
		exit_code = bcli_repo_init (value1);
	} else if (g_strcmp0 (command, "repo-add") == 0) {
		exit_code = bcli_repo_add_package (value1, value2);
	} else if (g_strcmp0 (command, "run") == 0) {
		exit_code = bcli_execute_build (value1);
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
