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

#define _g_free0(var) (var = (g_free (var), NULL))
#define _g_error_free0(var) ((var == NULL) ? NULL : (var = (g_error_free (var), NULL)))
#define _g_object_unref0(var) ((var == NULL) ? NULL : (var = (g_object_unref (var), NULL)))
#define _g_string_free0(var) ((var == NULL) ? NULL : (var = (g_string_free (var, TRUE), NULL)))
#define _g_hash_table_unref0(var) ((var == NULL) ? NULL : (var = (g_hash_table_unref (var), NULL)))
#define _g_regex_unref0(var) ((var == NULL) ? NULL : (var = (g_regex_unref (var), NULL)))
#define _g_list_free0(var) ((var == NULL) ? NULL : (var = (g_list_free (var), NULL)))

/**
 * bl_array_destroy:
 */
static void
bl_array_destroy (gpointer array, gint array_length, GDestroyNotify destroy_func)
{
	if ((array != NULL) && (destroy_func != NULL)) {
		int i;
		for (i = 0; i < array_length; i = i + 1) {
			if (((gpointer*) array)[i] != NULL) {
				destroy_func (((gpointer*) array)[i]);
			}
		}
	}
}

/**
 * bl_array_free:
 */
static void
bl_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func)
{
	bl_array_destroy (array, array_length, destroy_func);
	g_free (array);
}

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
	gchar* minimumVersion = NULL;
	gchar* filename = NULL;
	gint fd = 0;
	struct stat stat = {0};
	gint modificationTime = 0;

	gchar* firstLine = NULL;
	gchar* content = NULL;
	GString* headerFile = NULL;

	GFile* libPath = NULL;
	GFileEnumerator* enumerator = NULL;
	gint counter = 0;
	GHashTable* symbolMap = NULL;
	GHashTable* symbolsNewerThanMinimum = NULL;
	GList* keys = NULL;
	gboolean ret;
	gchar *tmp;
	GError *inner_error = NULL;

	if (argc != 3) {
		fprintf (stdout, "Usage: buildlist <output path of apsymbols.h> <minimum glibc version>\n");
		return 1;
	}

	minimumVersion = g_strdup (argv[2]);
	filename = g_strconcat (argv[1], "/apsymbols.h", NULL);

	/* We need to check if the buildlist executable changed. */
	fd = open (argv[0], 0, (mode_t) 0);
	fstat (fd, &stat);
	modificationTime = (gint) stat.st_mtime;
	close (fd);

	firstLine = g_strdup_printf ("/* minimum glibc %s; modification time of buildlist %d */", minimumVersion, modificationTime);

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
			g_free (minimumVersion);

			/* Don't generate it again */
			return 0;
		}

		g_strfreev (split);
	}


	headerFile = g_string_new ("");
	tmp = g_strconcat (firstLine, "\n", NULL);
	g_string_append (headerFile, tmp);
	g_free (tmp);

	fprintf (stdout, "Generating %s (glibc %s) .", filename, minimumVersion);
	fflush (stdout);

	libPath = g_file_new_for_path ("/lib/");

	enumerator = g_file_enumerate_children (libPath, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &inner_error);
	if (G_UNLIKELY (inner_error != NULL)) {
		goto out;
	}

	counter = 0;
	symbolMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	symbolsNewerThanMinimum = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	while (TRUE) {
		GFileInfo* fi = NULL;
		gchar* output = NULL;
		gchar* errorOutput = NULL;
		gint returnCode = 0;

		fi = g_file_enumerator_next_file (enumerator, NULL, &inner_error);
		if (G_UNLIKELY (inner_error != NULL)) {
			g_object_unref (fi);
			g_free (errorOutput);
			g_free (output);

			goto out;
		}

		counter++;
		if ((counter % 50) == 0) {
			fprintf (stdout, ".");
			fflush (stdout);
		}

		tmp = g_strconcat ("objdump -T /lib/", g_file_info_get_name (fi), NULL);
		g_spawn_command_line_sync (tmp, &output, &errorOutput, &returnCode, &inner_error);
		g_free (tmp);

		if (G_UNLIKELY (inner_error != NULL)) {
			g_object_unref (fi);
			g_free (errorOutput);
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
				line = g_strdup (split[line_it]);
				
				{
					GRegex* regex = NULL;
					GRegex* _tmp69_ = NULL;
					gboolean _tmp70_ = FALSE;
					GRegex* _tmp71_ = NULL;
					const gchar* _tmp72_ = NULL;
					gboolean _tmp73_ = FALSE;
					_tmp69_ = g_regex_new ("(.*)(GLIBC_)([[:digit:]]\\.([[:digit:]]\\.)*[[:digit:]])(\\)?)([ ]*)(.+)", 0, 0, &inner_error);
					regex = _tmp69_;
					if (G_UNLIKELY (inner_error != NULL)) {
						_g_free0 (line);
						split = (bl_array_free (split, split_len, (GDestroyNotify) g_free), NULL);
						_g_object_unref0 (fi);
						_g_hash_table_unref0 (symbolsNewerThanMinimum);
						_g_hash_table_unref0 (symbolMap);
						_g_object_unref0 (fi);
						_g_object_unref0 (enumerator);
						_g_object_unref0 (libPath);
						_g_free0 (errorOutput);
						_g_free0 (output);
						_g_string_free0 (headerFile);
						_g_free0 (content);
						_g_free0 (firstLine);
						_g_free0 (filename);
						_g_free0 (minimumVersion);
						goto out;
					}
					_tmp71_ = regex;
					_tmp72_ = line;
					_tmp73_ = g_regex_match (_tmp71_, _tmp72_, 0, NULL);
					if (_tmp73_) {
						const gchar* _tmp74_ = NULL;
						gboolean _tmp75_ = FALSE;
						_tmp74_ = line;
						_tmp75_ = string_contains (_tmp74_, "PRIVATE");
						_tmp70_ = !_tmp75_;
					} else {
						_tmp70_ = FALSE;
					}
					if (_tmp70_) {
						gchar* version = NULL;
						GRegex* _tmp76_ = NULL;
						const gchar* _tmp77_ = NULL;
						gchar** _tmp78_ = NULL;
						gchar** _tmp79_ = NULL;
						gchar** _tmp80_ = NULL;
						gint _tmp80__length1 = 0;
						const gchar* _tmp81_ = NULL;
						gchar* _tmp82_ = NULL;
						gchar* _tmp83_ = NULL;
						gchar* symbolName = NULL;
						GRegex* _tmp84_ = NULL;
						const gchar* _tmp85_ = NULL;
						gchar** _tmp86_ = NULL;
						gchar** _tmp87_ = NULL;
						gchar** _tmp88_ = NULL;
						gint _tmp88__length1 = 0;
						const gchar* _tmp89_ = NULL;
						gchar* _tmp90_ = NULL;
						gchar* _tmp91_ = NULL;
						gchar* versionInMap = NULL;
						GHashTable* _tmp92_ = NULL;
						const gchar* _tmp93_ = NULL;
						gconstpointer _tmp94_ = NULL;
						gchar* _tmp95_ = NULL;
						const gchar* _tmp96_ = NULL;
						const gchar* _tmp99_ = NULL;
						const gchar* _tmp133_ = NULL;
						const gchar* _tmp134_ = NULL;
						gint _tmp135_ = 0;
						_tmp76_ = regex;
						_tmp77_ = line;
						_tmp79_ = _tmp78_ = g_regex_split (_tmp76_, _tmp77_, 0);
						_tmp80_ = _tmp79_;
						_tmp80__length1 = bl_array_length (_tmp78_);
						_tmp81_ = _tmp80_[3];
						_tmp82_ = g_strdup (_tmp81_);
						_tmp83_ = _tmp82_;
						_tmp80_ = (bl_array_free (_tmp80_, _tmp80__length1, (GDestroyNotify) g_free), NULL);
						version = _tmp83_;
						_tmp84_ = regex;
						_tmp85_ = line;
						_tmp87_ = _tmp86_ = g_regex_split (_tmp84_, _tmp85_, 0);
						_tmp88_ = _tmp87_;
						_tmp88__length1 = bl_array_length (_tmp86_);
						_tmp89_ = _tmp88_[7];
						_tmp90_ = g_strdup (_tmp89_);
						_tmp91_ = _tmp90_;
						_tmp88_ = (bl_array_free (_tmp88_, _tmp88__length1, (GDestroyNotify) g_free), NULL);
						symbolName = _tmp91_;
						_tmp92_ = symbolMap;
						_tmp93_ = symbolName;
						_tmp94_ = g_hash_table_lookup (_tmp92_, _tmp93_);
						_tmp95_ = g_strdup ((const gchar*) _tmp94_);
						versionInMap = _tmp95_;
						_tmp96_ = symbolName;
						if (g_strcmp0 (_tmp96_, "2") == 0) {
							FILE* _tmp97_ = NULL;
							const gchar* _tmp98_ = NULL;
							_tmp97_ = stdout;
							_tmp98_ = line;
							fprintf (_tmp97_, "%s\n", _tmp98_);
						}
						_tmp99_ = versionInMap;
						if (_tmp99_ == NULL) {
							GHashTable* _tmp100_ = NULL;
							const gchar* _tmp101_ = NULL;
							gchar* _tmp102_ = NULL;
							const gchar* _tmp103_ = NULL;
							gchar* _tmp104_ = NULL;
							_tmp100_ = symbolMap;
							_tmp101_ = symbolName;
							_tmp102_ = g_strdup (_tmp101_);
							_tmp103_ = version;
							_tmp104_ = g_strdup (_tmp103_);
							g_hash_table_insert (_tmp100_, _tmp102_, _tmp104_);
						} else {
							gboolean _tmp105_ = FALSE;
							const gchar* _tmp106_ = NULL;
							const gchar* _tmp107_ = NULL;
							gint _tmp108_ = 0;
							gboolean _tmp117_ = FALSE;
							gboolean _tmp118_ = FALSE;
							const gchar* _tmp119_ = NULL;
							const gchar* _tmp120_ = NULL;
							gint _tmp121_ = 0;
							_tmp106_ = versionInMap;
							_tmp107_ = minimumVersion;
							_tmp108_ = li_compare_versions (_tmp106_, _tmp107_);
							if (_tmp108_ > 0) {
								const gchar* _tmp109_ = NULL;
								const gchar* _tmp110_ = NULL;
								gint _tmp111_ = 0;
								_tmp109_ = versionInMap;
								_tmp110_ = version;
								_tmp111_ = li_compare_versions (_tmp109_, _tmp110_);
								_tmp105_ = _tmp111_ > 0;
							} else {
								_tmp105_ = FALSE;
							}
							if (_tmp105_) {
								GHashTable* _tmp112_ = NULL;
								const gchar* _tmp113_ = NULL;
								gchar* _tmp114_ = NULL;
								const gchar* _tmp115_ = NULL;
								gchar* _tmp116_ = NULL;
								_tmp112_ = symbolMap;
								_tmp113_ = symbolName;
								_tmp114_ = g_strdup (_tmp113_);
								_tmp115_ = version;
								_tmp116_ = g_strdup (_tmp115_);
								g_hash_table_insert (_tmp112_, _tmp114_, _tmp116_);
							}
							_tmp119_ = minimumVersion;
							_tmp120_ = versionInMap;
							_tmp121_ = li_compare_versions (_tmp119_, _tmp120_);
							if (_tmp121_ > 0) {
								const gchar* _tmp122_ = NULL;
								const gchar* _tmp123_ = NULL;
								gint _tmp124_ = 0;
								_tmp122_ = version;
								_tmp123_ = versionInMap;
								_tmp124_ = li_compare_versions (_tmp122_, _tmp123_);
								_tmp118_ = _tmp124_ > 0;
							} else {
								_tmp118_ = FALSE;
							}

							if (_tmp118_) {
								const gchar* _tmp125_ = NULL;
								const gchar* _tmp126_ = NULL;
								gint _tmp127_ = 0;
								_tmp125_ = minimumVersion;
								_tmp126_ = version;
								_tmp127_ = li_compare_versions (_tmp125_, _tmp126_);
								_tmp117_ = _tmp127_ > 0;
							} else {
								_tmp117_ = FALSE;
							}
							if (_tmp117_) {
								GHashTable* _tmp128_ = NULL;
								const gchar* _tmp129_ = NULL;
								gchar* _tmp130_ = NULL;
								const gchar* _tmp131_ = NULL;
								gchar* _tmp132_ = NULL;
								_tmp128_ = symbolMap;
								_tmp129_ = symbolName;
								_tmp130_ = g_strdup (_tmp129_);
								_tmp131_ = version;
								_tmp132_ = g_strdup (_tmp131_);
								g_hash_table_insert (_tmp128_, _tmp130_, _tmp132_);
							}
						}
						_tmp133_ = version;
						_tmp134_ = minimumVersion;
						_tmp135_ = li_compare_versions (_tmp133_, _tmp134_);
						if (_tmp135_ > 0) {
							GHashTable* _tmp136_ = NULL;
							const gchar* _tmp137_ = NULL;
							gchar* _tmp138_ = NULL;
							_tmp136_ = symbolsNewerThanMinimum;
							_tmp137_ = symbolName;
							_tmp138_ = g_strdup (_tmp137_);
							g_hash_table_add (_tmp136_, _tmp138_);
						}
						_g_free0 (versionInMap);
						_g_free0 (symbolName);
						_g_free0 (version);
					}
					_g_regex_unref0 (regex);
					_g_free0 (line);
				}
			}
			split = (bl_array_free (split, split_len, (GDestroyNotify) g_free), NULL);
		}
		g_object_unref (fi);
	}

	g_string_append (headerFile, "/* libuild embedded metadata */\n" \
		"#define LIBUILD_NOTE_METADATA(s)   __asm__(\".section .metadata, \\\"M" \
		"S\\\", @note, 1\\n\\t.string \\\"\" s \"\\\"\\n\\t.previous\\n\\t\")\n" \
		"\n" \
		"#ifdef LIBUILD_VERSION\n" \
		"LIBUILD_NOTE_METADATA(\"libuild.version=\" LIBUILD_VERSION);\n" \
		"#endif\n" \
		"\n" \
		"/* libuild generated symbol exclusion list */\n");

	keys = g_hash_table_get_keys (symbolMap);
	{
		GList* sym_collection = NULL;
		GList* sym_it = NULL;
		sym_collection = keys;
		for (sym_it = sym_collection; sym_it != NULL; sym_it = sym_it->next) {
			gchar* _tmp143_ = NULL;
			gchar* sym = NULL;
			_tmp143_ = g_strdup ((const gchar*) sym_it->data);
			sym = _tmp143_;
			{
				GHashTable* _tmp144_ = NULL;
				const gchar* _tmp145_ = NULL;
				gboolean _tmp146_ = FALSE;
				gchar* version = NULL;
				GHashTable* _tmp147_ = NULL;
				const gchar* _tmp148_ = NULL;
				gconstpointer _tmp149_ = NULL;
				gchar* _tmp150_ = NULL;
				gchar* versionToUse = NULL;
				const gchar* _tmp151_ = NULL;
				gchar* _tmp152_ = NULL;
				const gchar* _tmp153_ = NULL;
				const gchar* _tmp154_ = NULL;
				gint _tmp155_ = 0;
				GString* _tmp158_ = NULL;
				const gchar* _tmp159_ = NULL;
				const gchar* _tmp160_ = NULL;
				const gchar* _tmp161_ = NULL;
				gchar* _tmp162_ = NULL;
				gchar* _tmp163_ = NULL;
				_tmp144_ = symbolsNewerThanMinimum;
				_tmp145_ = sym;
				_tmp146_ = g_hash_table_contains (_tmp144_, _tmp145_);
				if (!_tmp146_) {
						_g_free0 (sym);
						continue;
					}
				_tmp147_ = symbolMap;
				_tmp148_ = sym;
				_tmp149_ = g_hash_table_lookup (_tmp147_, _tmp148_);
				_tmp150_ = g_strdup ((const gchar*) _tmp149_);
				version = _tmp150_;
				_tmp151_ = version;
				_tmp152_ = g_strdup (_tmp151_);
				versionToUse = _tmp152_;
				_tmp153_ = version;
				_tmp154_ = minimumVersion;
				_tmp155_ = li_compare_versions (_tmp153_, _tmp154_);
				if (_tmp155_ > 0) {
					const gchar* _tmp156_ = NULL;
					gchar* _tmp157_ = NULL;
					_tmp156_ = version;
					_tmp157_ = g_strdup_printf ("DONT_USE_THIS_VERSION_%s", _tmp156_);
					_g_free0 (versionToUse);
					versionToUse = _tmp157_;
				}
				_tmp158_ = headerFile;
				_tmp159_ = sym;
				_tmp160_ = sym;
				_tmp161_ = versionToUse;
				_tmp162_ = g_strdup_printf ("__asm__(\".symver %s, %s@GLIBC_%s\");\n", _tmp159_, _tmp160_, _tmp161_);
				_tmp163_ = _tmp162_;
				g_string_append (_tmp158_, _tmp163_);
				_g_free0 (_tmp163_);
				_g_free0 (versionToUse);
				_g_free0 (version);
				_g_free0 (sym);
			}
		}
	}

	g_file_set_contents (filename, headerFile->str, (gssize) (-1), &inner_error);
	if (G_UNLIKELY (inner_error != NULL)) {
		goto out;
	}

out:
	g_list_free (keys);
	g_hash_table_unref (symbolsNewerThanMinimum);
	g_hash_table_unref (symbolMap);
	g_object_unref (enumerator);
	g_object_unref (libPath);
	g_string_free (headerFile, TRUE);
	g_free (content);
	g_free (firstLine);
	g_free (filename);
	g_free (minimumVersion);
	g_object_unref (enumerator);
	g_object_unref (libPath);

	if (inner_error == NULL) {
		fprintf (stdout, " OK\n");

		return 0;
	} else {
		g_warning ("%s", inner_error->message);
		g_error_free (inner_error);

		return 1;
	}
}
