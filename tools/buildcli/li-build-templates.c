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
#include "li-build-templates.h"

#include <stdio.h>
#include <locale.h>
#include <glib/gi18n-lib.h>
#include <limba.h>
#include <appstream.h>

#include "li-console-utils.h"

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
 * libuild_make_template:
 */
gint
libuild_make_template (const gchar *dir)
{
	gint res = 0;
	gchar *res_dir = NULL;
	gchar *tmp;
	gchar *fname;
	gboolean appstream_linked = FALSE;
	GError *error = NULL;

	if (dir == NULL) {
		tmp = g_get_current_dir ();
		res_dir = g_build_filename (tmp, "lipkg", NULL);
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
		AsMetadata *metad;
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
				as_component_set_name (cpt, tmp, NULL);
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
				as_component_set_summary (cpt, tmp, NULL);
				g_free (tmp);
				break;
			}
		}

		asfile = g_build_filename (res_dir, "metainfo.xml", NULL);
		metad = as_metadata_new ();
		as_metadata_add_component (metad, cpt);
		tmp = as_metadata_component_to_upstream_xml (metad);
		g_object_unref (metad);
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
"and install it into the 'target' subdirectory of your 'lipkg' directory.\n" \
"Then run 'lipkgen build lipkg/' to create your package. \n" \
"If you want to embed dependencies, place their IPK packages in the 'repo/'\n" \
"subdirectory of 'lipkg/'"), res_dir, LI_SW_ROOT_PREFIX);
	li_print_stdout ("========\n");

out:
	g_free (res_dir);
	return res;
}
