/* buildlist.c -- Generate apsymbols.h file
 *
 * Copyright (C) 2009-2010 Jan Niklas Hasse <jhasse@gmail.com>
 * Copyright (C) 2010-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <limba.h>

/**
 * bl_array_length:
 */
static gint
bl_array_length (gpointer array) {
	int length;

	length = 0;
	if (array) {
		while (((gpointer*) array)[length]) {
			length++;
		}
	}

	return length;
}

/**
 * string_contains:
 */
static gboolean
string_contains (const gchar *str, const gchar* needle)
{
	gboolean ret = FALSE;
	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (needle != NULL, FALSE);

	ret = strstr ((gchar*) str, (gchar*) needle) != NULL;
	return ret;
}

/**
 * main:
 */
int main (int argc, char **argv)
{
	gchar* minimum_version = NULL;
	gchar* filename = NULL;

	gchar* firstLine = NULL;
	gchar* content = NULL;
	GString* header_content = NULL;

	GFile* libPath = NULL;
	GFileEnumerator* enumerator = NULL;
	gint counter = 0;

	GHashTable* symbol_map = NULL;
	GHashTable* symbols_newer_than_minimum = NULL;

	gint fd = 0;
	struct stat stat = {0};
	gint modificationTime = 0;
	GList* keys = NULL;
	gboolean ret;
	gchar *tmp;
	GList* sym_it = NULL;
	GError *inner_error = NULL;

	if (argc != 3) {
		fprintf (stdout, "Usage: buildlist <output path of apsymbols.h> <minimum glibc version>\n");
		return 1;
	}

	minimum_version = g_strdup (argv[2]);
	filename = g_strconcat (argv[1], "/apsymbols.h", NULL);

	/* We need to check if the buildlist executable changed. */
	fd = open (argv[0], 0, (mode_t) 0);
	if (fd >= 0) {
		if (fstat (fd, &stat) == 0)
			modificationTime = (gint) stat.st_mtime;
		else
			g_debug ("Unable to get mtime for self.");
		close (fd);
	} else {
		g_debug ("Unable to get mtime for self: Could not get fd.");
	}

	firstLine = g_strdup_printf ("/* minimum glibc %s; modification time of buildlist %d */", minimum_version, modificationTime);

	/* TODO: Don't open the whole file just to read the first line */
	ret = g_file_get_contents (filename, &content, NULL, NULL);

	if (ret) {
		gchar** split = NULL;

		split = g_strsplit (content, "\n", 0);

		/* Is this already the correct file? */
		if (g_strcmp0 (split[0], firstLine) == 0) {
			g_strfreev (split);
			g_free (content);
			g_free (firstLine);
			g_free (filename);
			g_free (minimum_version);

			/* Don't generate it again */
			return 0;
		}

		g_strfreev (split);
	}

	header_content = g_string_new ("");
	tmp = g_strconcat (firstLine, "\n", NULL);
	g_string_append (header_content, tmp);
	g_free (tmp);

	fprintf (stdout, "Generating %s (glibc %s) .", filename, minimum_version);
	fflush (stdout);

	libPath = g_file_new_for_path ("/lib/");

	enumerator = g_file_enumerate_children (libPath, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &inner_error);
	if (G_UNLIKELY (inner_error != NULL)) {
		goto out;
	}

	/* This map contains every symbol and the version as close to the minimum version as possible */
	symbol_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	/* This set contains all symbols used by glibc versions newer than minimum_version */
	symbols_newer_than_minimum = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	counter = 0;
	while (TRUE) {
		GFileInfo* fi = NULL;
		gchar* output = NULL;
		gchar* output_error = NULL;
		gint returnCode = 0;

		fi = g_file_enumerator_next_file (enumerator, NULL, &inner_error);
		if (fi == NULL)
			break;

		if (G_UNLIKELY (inner_error != NULL)) {
			g_object_unref (fi);

			goto out;
		}

		counter++;
		if ((counter % 50) == 0) {
			fprintf (stdout, ".");
			fflush (stdout);
		}

		tmp = g_strconcat ("objdump -T /lib/", g_file_info_get_name (fi), NULL);
		g_spawn_command_line_sync (tmp, &output, &output_error, &returnCode, &inner_error);
		g_free (tmp);

		if (G_UNLIKELY (inner_error != NULL)) {
			g_object_unref (fi);
			g_free (output_error);
			g_free (output);

			goto out;
		}

		if (returnCode == 0) {
			gchar** split = NULL;
			gint split_len = 0;
			gint line_it = 0;

			split = g_strsplit (output, "\n", 0);

			split_len = bl_array_length (split);
			for (line_it = 0; line_it < split_len; line_it = line_it + 1) {
				gchar* line = NULL;
				GRegex* regex = NULL;

				line = g_strdup (split[line_it]);

				regex = g_regex_new ("(.*)(GLIBC_)([[:digit:]]\\.([[:digit:]]\\.)*[[:digit:]])(\\)?)([ ]*)(.+)", 0, 0, &inner_error);
				if (G_UNLIKELY (inner_error != NULL)) {
					g_free (line);
					g_strfreev (split);
					g_regex_unref (regex);

					goto out;
				}

				if ((g_regex_match (regex, line, 0, NULL)) && (!string_contains (line, "PRIVATE"))) {
					gchar* version = NULL;
					gchar* symbol_name = NULL;
					gchar* version_in_map = NULL;
					gchar **strv;

					strv = g_regex_split (regex, line, 0);
					version = g_strdup (strv[3]);
					symbol_name = g_strdup (strv[7]);
					g_strfreev (strv);


					version_in_map = g_strdup ((const gchar*) g_hash_table_lookup (symbol_map, symbol_name));
					if (g_strcmp0 (symbol_name, "2") == 0) {
						fprintf (stdout, "%s\n", line);
					}

					if (version_in_map == NULL) {
						g_hash_table_insert (symbol_map,
										g_strdup (symbol_name),
										g_strdup (version));
					} else {
						/* Is this version older then the version in the map? */
						if ((li_compare_versions (version_in_map, minimum_version) > 0) && (li_compare_versions (version_in_map, version) > 0)) {
							g_hash_table_insert (symbol_map,
											g_strdup (symbol_name),
											g_strdup (version));
						}

						/* If the version in the map is already older then the minimum version, we should only add newer versions */
						if ((li_compare_versions (minimum_version, version_in_map) > 0) && (li_compare_versions (version, version_in_map) > 0) && (li_compare_versions (minimum_version, version) > 0)) {
							g_hash_table_insert (symbol_map,
											g_strdup (symbol_name),
											g_strdup (version));
						}
					}

					if (li_compare_versions (version, minimum_version) > 0) {
						g_hash_table_add (symbols_newer_than_minimum, g_strdup (symbol_name));
					}

					g_free (version_in_map);
					g_free (symbol_name);
					g_free (version);
				}

				g_regex_unref (regex);
				g_free (line);
			}

			g_strfreev (split);
		}

		g_free (output);
		g_free (output_error);
		g_object_unref (fi);
	}

	g_string_append (header_content, "/* libuild embedded metadata */\n" \
		"#define LIBUILD_NOTE_METADATA(s)   __asm__(\".section .metadata, \\\"M" \
		"S\\\", @note, 1\\n\\t.string \\\"\" s \"\\\"\\n\\t.previous\\n\\t\")\n" \
		"\n" \
		"#ifdef LIBUILD_VERSION\n" \
		"LIBUILD_NOTE_METADATA(\"libuild.version=\" LIBUILD_VERSION);\n" \
		"#endif\n" \
		"\n" \
		"/* libuild generated symbol exclusion list */\n");

	keys = g_hash_table_get_keys (symbol_map);
	for (sym_it = keys; sym_it != NULL; sym_it = sym_it->next) {
		const gchar *sym;
		gchar* version = NULL;
		gchar* version_to_use = NULL;
		sym = (const gchar*) sym_it->data;

		/* Remove all symbols which aren't obsoleted by newer versions */
		if (!g_hash_table_contains (symbols_newer_than_minimum, sym)) {
			continue;
		}

		version = g_strdup ((const gchar*) g_hash_table_lookup (symbol_map, sym));
		version_to_use = g_strdup (version);

		if (li_compare_versions (version, minimum_version) > 0) {
			g_free (version_to_use);
			version_to_use = g_strdup_printf ("DO_NOT_USE_THIS_VERSION_%s", version);
		}

		tmp = g_strdup_printf ("__asm__(\".symver %s, %s@GLIBC_%s\");\n", sym, sym, version_to_use);
		g_string_append (header_content, tmp);
		g_free (tmp);

		g_free (version_to_use);
		g_free (version);
	}

	g_file_set_contents (filename, header_content->str, (gssize) (-1), &inner_error);
	if (G_UNLIKELY (inner_error != NULL)) {
		goto out;
	}

out:
	g_list_free (keys);
	g_hash_table_unref (symbols_newer_than_minimum);
	g_hash_table_unref (symbol_map);
	g_object_unref (enumerator);
	g_object_unref (libPath);
	g_string_free (header_content, TRUE);
	g_free (content);
	g_free (firstLine);
	g_free (filename);
	g_free (minimum_version);

	if (inner_error == NULL) {
		fprintf (stdout, " OK\n");

		return 0;
	} else {
		g_warning ("%s", inner_error->message);
		g_error_free (inner_error);

		return 1;
	}
}
