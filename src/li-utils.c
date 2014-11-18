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
#include "li-utils-private.h"

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <uuid/uuid.h>
#include <appstream.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

/**
 * SECTION:li-utils
 * @short_description: General-purpose helper functions for Limba
 * @include: limba.h
 */

/* set to TRUE in case we are running unit tests */
static gboolean _unittestmode = FALSE;

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
 * li_copy_file:
 */
gboolean
li_copy_file (const gchar *source, const gchar *destination, GError **error)
{
	FILE *fsrc, *fdest;
	int a;

	fsrc = fopen (source, "rb");
	if (fsrc == NULL) {
		g_set_error (error,
				G_FILE_ERROR,
				G_FILE_ERROR_FAILED,
				"Could not copy file: %s", g_strerror (errno));
		return FALSE;
	}

	fdest = fopen (destination, "wb");
	if (fdest == NULL) {
		g_set_error (error,
				G_FILE_ERROR,
				G_FILE_ERROR_FAILED,
				"Could not copy file: %s", g_strerror (errno));
		return FALSE;
	}

	while (TRUE) {
		a = fgetc (fsrc);

		if (!feof (fsrc))
			fputc (a, fdest);
		else
			break;
	}

	fclose (fdest);
	fclose (fsrc);
	return TRUE;
}

/**
 * li_delete_dir_recursive:
 * @dirname: Directory to remove
 *
 * Remove folder like rm -r does
 *
 * Returns: TRUE if operation was successful
 */
gboolean
li_delete_dir_recursive (const gchar* dirname)
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
			li_delete_dir_recursive (path);
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
		if ((g_file_test (path, G_FILE_TEST_IS_DIR)) && (recursive)) {
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
 * li_get_last_version_from_component:
 */
const gchar*
li_get_last_version_from_component (AsComponent *cpt)
{
	GPtrArray *releases;
	AsRelease *release = NULL;
	guint64 timestamp = 0;
	guint i;
	const gchar *version = NULL;

	releases = as_component_get_releases (cpt);
	for (i = 0; i < releases->len; i++) {
		AsRelease *r = AS_RELEASE (g_ptr_array_index (releases, i));
		if (as_release_get_timestamp (r) >= timestamp) {
				release = r;
				timestamp = as_release_get_timestamp (r);
		}
	}
	if (release != NULL) {
		version = as_release_get_version (release);
	}

	return version;
}

/**
 * li_utils_get_tmp_dir:
 */
gchar*
li_utils_get_tmp_dir (const gchar *prefix)
{
	gchar *template;
	gchar *path;
	gchar *tmp_dir = NULL;
	const gchar *tmp_root_path = "/var/tmp/limba";

	g_mkdir_with_parents (tmp_root_path, 0755);

	template = g_strdup_printf ("%s-XXXXXX", prefix);
	/* create temporary directory */
	path = g_build_filename (tmp_root_path, template, NULL);
	g_free (template);

	tmp_dir = mkdtemp (path);
	if (tmp_dir == NULL) {
		g_critical ("Unable to create temporary directory! Error: %s", g_strerror (errno));
		tmp_dir = path;
	}
	tmp_dir = g_strdup (tmp_dir);
	g_free (path);

	return tmp_dir;
}

/**
 * li_get_uuid_string:
 */
gchar*
li_get_uuid_string ()
{
	uuid_t uuid;
	char uuid_str[37];

	uuid_generate_time_safe(uuid);
	uuid_unparse_lower(uuid, uuid_str);

	return g_strndup (uuid_str, 36);
}

/**
 * li_get_install_root:
 *
 * A hack to support unit-tests running as non-root.
 */
const gchar*
li_get_software_root ()
{
	if (_unittestmode) {
		const gchar *tmpdir = "/var/tmp/limba/test-root/opt/software";
		g_mkdir_with_parents (tmpdir, 0755);
		return tmpdir;
	} else {
		return LI_SU_SOFTWARE_ROOT;
	}
}

/**
 * li_get_prefixdir:
 *
 * A hack to support unit-tests running as non-root.
 */
const gchar*
li_get_prefixdir ()
{
	if (_unittestmode) {
		#define tmpdir "/var/tmp/limba/test-root/usr"
		g_mkdir_with_parents (tmpdir "/share/applications", 0755);
		g_mkdir_with_parents (tmpdir "/local/bin", 0755);
		return tmpdir;
	} else {
		return PREFIXDIR;
	}
}

/**
 * li_set_unittestmode:
 */
void
li_set_unittestmode (gboolean testmode)
{
	_unittestmode = testmode;
}

/**
 * li_set_verbose:
 * @verbose: %TRUE to increase verbosity
 *
 * Write verbose output on the command line.
 */
void
li_set_verbose (gboolean verbose)
{
	/* TODO: Replace this hack with a logging handler */
	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
}
