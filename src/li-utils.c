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
#include <sys/utsname.h>
#include <errno.h>

/**
 * SECTION:li-utils
 * @short_description: General-purpose helper functions for Limba
 * @include: limba.h
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
		fclose (fsrc);
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

	fclose (input);
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

	g_mkdir_with_parents (tmp_root_path, 0777);

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
	return LI_SOFTWARE_ROOT;
}

/**
 * li_get_current_arch_h:
 *
 * Get the current architecture in a human-friendly form
 * (e.g. "amd64" instead of "x86_64").
 *
 * Returns: (transfer full): The current OS architecture as string
 */
gchar*
li_get_current_arch_h (void)
{
	gchar *arch;
	struct utsname uts;

	uname (&uts);

	if (g_strcmp0 (uts.machine, "x86_64") == 0) {
		arch = g_strdup ("amd64");
	} else if (g_pattern_match_simple ("i?86", uts.machine)) {
		arch = g_strdup ("ia32");
	} else if (g_strcmp0 (uts.machine, "aarch64")) {
		arch = g_strdup ("arm64");
	} else {
		arch = g_strdup (uts.machine);
	}

	return arch;
}

/**
 * li_set_verbose_mode:
 * @verbose: %TRUE to increase verbosity
 *
 * Write verbose output on the command line.
 */
void
li_set_verbose_mode (gboolean verbose)
{
	/* TODO: Replace this hack with a logging handler */
	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
}

/**
 * li_compare_versions:
 *
 * Compare alpha and numeric segments of two versions.
 * This algorithm is also used in RPM and licensed under a GPLv2+
 * license.
 *
 * Returns: 1: a is newer than b
 *		  0: a and b are the same version
 *		 -1: b is newer than a
 */
gint
li_compare_versions (const gchar* a, const gchar *b)
{
	/* easy comparison to see if versions are identical */
	if (g_strcmp0 (a, b) == 0)
		return 0;

	gchar oldch1, oldch2;
	gchar abuf[strlen(a)+1], bbuf[strlen(b)+1];
	gchar *str1 = abuf, *str2 = bbuf;
	gchar *one, *two;
	int rc;
	gboolean isnum;

	strcpy(str1, a);
	strcpy(str2, b);

	one = str1;
	two = str2;

	/* loop through each version segment of str1 and str2 and compare them */
	while (*one || *two) {
		while (*one && !g_ascii_isalnum (*one) && *one != '~') one++;
		while (*two && !g_ascii_isalnum (*two) && *two != '~') two++;

		/* handle the tilde separator, it sorts before everything else */
		if (*one == '~' || *two == '~') {
			if (*one != '~') return 1;
			if (*two != '~') return -1;
			one++;
			two++;
			continue;
		}

		/* If we ran to the end of either, we are finished with the loop */
		if (!(*one && *two)) break;

		str1 = one;
		str2 = two;

		/* grab first completely alpha or completely numeric segment */
		/* leave one and two pointing to the start of the alpha or numeric */
		/* segment and walk str1 and str2 to end of segment */
		if (g_ascii_isdigit (*str1)) {
			while (*str1 && g_ascii_isdigit (*str1)) str1++;
			while (*str2 && g_ascii_isdigit (*str2)) str2++;
			isnum = TRUE;
		} else {
			while (*str1 && g_ascii_isalpha (*str1)) str1++;
			while (*str2 && g_ascii_isalpha (*str2)) str2++;
			isnum = FALSE;
		}

		/* save character at the end of the alpha or numeric segment */
		/* so that they can be restored after the comparison */
		oldch1 = *str1;
		*str1 = '\0';
		oldch2 = *str2;
		*str2 = '\0';

		/* this cannot happen, as we previously tested to make sure that */
		/* the first string has a non-null segment */
		if (one == str1) return -1;	/* arbitrary */

		/* take care of the case where the two version segments are */
		/* different types: one numeric, the other alpha (i.e. empty) */
		/* numeric segments are always newer than alpha segments */
		if (two == str2) return (isnum ? 1 : -1);

		if (isnum) {
			size_t onelen, twolen;
			/* this used to be done by converting the digit segments */
			/* to ints using atoi() - it's changed because long  */
			/* digit segments can overflow an int - this should fix that. */

			/* throw away any leading zeros - it's a number, right? */
			while (*one == '0') one++;
			while (*two == '0') two++;

			/* whichever number has more digits wins */
			onelen = strlen (one);
			twolen = strlen (two);
			if (onelen > twolen) return 1;
			if (twolen > onelen) return -1;
		}

		/* strcmp will return which one is greater - even if the two */
		/* segments are alpha or if they are numeric.  don't return  */
		/* if they are equal because there might be more segments to */
		/* compare */
		rc = strcmp (one, two);
		if (rc) return (rc < 1 ? -1 : 1);

		/* restore character that was replaced by null above */
		*str1 = oldch1;
		one = str1;
		*str2 = oldch2;
		two = str2;
	}

	/* this catches the case where all numeric and alpha segments have */
	/* compared identically but the segment sepparating characters were */
	/* different */
	if ((!*one) && (!*two)) return 0;

	/* whichever version still has characters left over wins */
	if (!*one) return -1; else return 1;
}
