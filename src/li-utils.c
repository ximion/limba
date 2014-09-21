/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2014 Matthias Klumpp <matthias@tenstral.net>
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

#include "li-utils.h"

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <sys/types.h>

/**
 * SECTION:li-utils
 * @short_description: General-purpose helper functions for Listaller
 * @include: listaller.h
 */

/**
 * li_str_empty:
 */
gboolean
li_str_empty (const gchar* str)
{
	if ((str == NULL) || (g_strcmp0 (str, "") == 0))
		return TRUE;
	return FALSE;
}

/**
 * li_utils_touch_dir:
 */
gboolean
li_utils_touch_dir (const gchar* dirname)
{
	GFile *d = NULL;
	GError *error = NULL;
	g_return_val_if_fail (dirname != NULL, FALSE);

	d = g_file_new_for_path (dirname);
	if (!g_file_query_exists (d, NULL)) {
		g_file_make_directory_with_parents (d, NULL, &error);
		if (error != NULL) {
			g_critical ("Unable to create directory tree. Error: %s", error->message);
			g_error_free (error);
			return FALSE;
		}
	}
	g_object_unref (d);

	return TRUE;
}

/**
 * li_utils_delete_dir_recursive:
 * @dirname: Directory to remove
 *
 * Remove folder like rm -r does
 *
 * Returns: TRUE if operation was successful
 */
gboolean
li_utils_delete_dir_recursive (const gchar* dirname)
{
	GError *error = NULL;
	gboolean ret = FALSE;
	GFile *dir;
	GFileEnumerator *enr;
	GFileInfo *info;
	g_return_val_if_fail (dirname != NULL, FALSE);

	if (!g_file_test (dirname, G_FILE_TEST_IS_DIR))
		return TRUE;

	dir = g_file_new_for_path (dirname);
	enr = g_file_enumerate_children (dir, "standard::name", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
	if (error != NULL)
		goto out;

	if (enr == NULL)
		goto out;
	info = g_file_enumerator_next_file (enr, NULL, &error);
	if (error != NULL)
		goto out;
	while (info != NULL) {
		gchar *path;
		path = g_build_filename (dirname, g_file_info_get_name (info), NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			li_utils_delete_dir_recursive (path);
		} else {
			g_remove (path);
		}
		g_object_unref (info);
		info = g_file_enumerator_next_file (enr, NULL, &error);
		if (error != NULL)
			goto out;
	}
	if (g_file_test (dirname, G_FILE_TEST_EXISTS)) {
		g_rmdir (dirname);
	}
	ret = TRUE;

out:
	g_object_unref (dir);
	if (enr != NULL)
		g_object_unref (enr);
	if (error != NULL) {
		g_critical ("Could not remove directory: %s", error->message);
		g_error_free (error);
	}
	return ret;
}

/**
 * li_utils_find_files_matching:
 */
GPtrArray*
li_utils_find_files_matching (const gchar* dir, const gchar* pattern, gboolean recursive)
{
	GPtrArray* list;
	GError *error = NULL;
	GFileInfo *file_info;
	GFileEnumerator *enumerator = NULL;
	GFile *fdir;
	g_return_val_if_fail (dir != NULL, NULL);
	g_return_val_if_fail (pattern != NULL, NULL);

	list = g_ptr_array_new_with_free_func (g_free);
	fdir =  g_file_new_for_path (dir);
	enumerator = g_file_enumerate_children (fdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
	if (error != NULL)
		goto out;

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL) {
		gchar *path;
		if (g_file_info_get_is_hidden (file_info))
			continue;
		path = g_build_filename (dir,
								 g_file_info_get_name (file_info),
								 NULL);
		if ((!g_file_test (path, G_FILE_TEST_IS_REGULAR)) && (recursive)) {
			GPtrArray *subdir_list;
			guint i;
			subdir_list = li_utils_find_files_matching (path, pattern, recursive);
			/* if there was an error, exit */
			if (subdir_list == NULL) {
				g_ptr_array_unref (list);
				list = NULL;
				g_free (path);
				goto out;
			}
			for (i=0; i<subdir_list->len; i++)
				g_ptr_array_add (list,
								 g_strdup ((gchar *) g_ptr_array_index (subdir_list, i)));
			g_ptr_array_unref (subdir_list);
		} else {
			if (!li_str_empty (pattern)) {
				if (!g_pattern_match_simple (pattern, g_file_info_get_name (file_info))) {
					g_free (path);
					continue;
				}
			}
			g_ptr_array_add (list, path);
		}
	}
	if (error != NULL)
		goto out;

out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
	if (error != NULL) {
		fprintf (stderr, "Error while finding files in directory %s: %s\n", dir, error->message);
		g_ptr_array_unref (list);
		return NULL;
	}

	return list;
}

/**
 * li_utils_find_files:
 */
GPtrArray*
li_utils_find_files (const gchar* dir, gboolean recursive)
{
	GPtrArray* res = NULL;
	g_return_val_if_fail (dir != NULL, NULL);

	res = li_utils_find_files_matching (dir, "", recursive);
	return res;
}

/**
 * li_utils_is_root:
 */
gboolean
li_utils_is_root (void)
{
	uid_t vuid;
	vuid = getuid ();
	return (vuid == ((uid_t) 0));
}

/**
 * li_string_strip:
 */
gchar*
li_string_strip (const gchar* str)
{
	gchar* result = NULL;
	gchar* _tmp0_ = NULL;
	g_return_val_if_fail (str != NULL, NULL);
	_tmp0_ = g_strdup (str);
	result = _tmp0_;
	g_strstrip (result);
	return result;
}

/**
 * li_ptr_array_to_strv:
 * @array: (element-type utf8)
 *
 * Returns: (transfer full): strv of the string array
 */
gchar**
li_ptr_array_to_strv (GPtrArray *array)
{
	gchar **value;
	const gchar *value_temp;
	guint i;

	g_return_val_if_fail (array != NULL, NULL);

	/* copy the array to a strv */
	value = g_new0 (gchar *, array->len + 1);
	for (i=0; i<array->len; i++) {
		value_temp = (const gchar *) g_ptr_array_index (array, i);
		value[i] = g_strdup (value_temp);
	}

	return value;
}

/**
 * li_str_replace:
 */
gchar*
li_str_replace (const gchar *str, const gchar *old, const gchar *new)
{
	gchar *ret, *r;
	const gchar *p, *q;
	size_t oldlen = strlen(old);
	size_t count, retlen, newlen = strlen(new);

	if (oldlen != newlen) {
		for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
			count++;
		/* this is undefined if p - str > PTRDIFF_MAX */
		retlen = p - str + strlen(p) + count * (newlen - oldlen);
	} else
		retlen = strlen(str);

	if ((ret = malloc(retlen + 1)) == NULL)
		return NULL;

	for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
		/* this is undefined if q - p > PTRDIFF_MAX */
		ptrdiff_t l = q - p;
		memcpy(r, p, l);
		r += l;
		memcpy(r, new, newlen);
		r += newlen;
	}
	strcpy(r, p);

	return ret;
}

/**
 * li_compute_checksum_for_file:
 *
 * Create a SHA256 checksum for the given file
 */
gchar*
li_compute_checksum_for_file (const gchar *fname)
{
	const GChecksumType cstype = G_CHECKSUM_SHA256;
	_cleanup_checksum_free_ GChecksum *cs;
	guchar data[4096] = {0};
	size_t size = 0;
	FILE *input;
	const gchar *sum;

	cs = g_checksum_new (cstype);
	input = fopen (fname, "rb");

	/* return NULL if we were unable to open the file */
	if (input == NULL)
		return NULL;

	/* build the checksum */
	do {
		size = read (fileno (input), (void*) data, (gsize) 4096);
		g_checksum_update (cs, data, size);
	} while (size == 4096);
	close (fileno (input));

	sum = g_checksum_get_string (cs);

	return g_strdup (sum);
}

/**
 * li_save_string_to_file:
 */
gboolean
li_save_string_to_file (const gchar *fname, const gchar *data, gboolean override, GError **error)
{
	_cleanup_object_unref_ GFile *file;
	_cleanup_object_unref_ GFileOutputStream *file_stream = NULL;
	_cleanup_object_unref_ GDataOutputStream *data_stream = NULL;

	file = g_file_new_for_path (fname);
	if ((!override) && (g_file_query_exists (file, NULL)))
		return FALSE;

	file_stream = g_file_create (file, G_FILE_CREATE_REPLACE_DESTINATION, NULL, error);
	if (error != NULL)
		return FALSE;
	data_stream = g_data_output_stream_new (G_OUTPUT_STREAM (file_stream));
	g_data_output_stream_put_string (data_stream, data, NULL, error);
	if (error != NULL)
		return FALSE;
	return TRUE;
}
