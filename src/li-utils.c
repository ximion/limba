/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2016 Matthias Klumpp <matthias@tenstral.net>
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
#include "li-systemd-dbus.h"

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
		g_autofree gchar *path = NULL;
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
	size_t oldlen = strlen (old);
	size_t count, retlen, newlen = strlen (new);

	if (oldlen != newlen) {
		for (count = 0, p = str; (q = strstr (p, old)) != NULL; p = q + oldlen)
			count++;
		/* this is undefined if p - str > PTRDIFF_MAX */
		retlen = p - str + strlen (p) + count * (newlen - oldlen);
	} else
		retlen = strlen (str);

	if ((ret = malloc (retlen + 1)) == NULL)
		return NULL;

	for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
		/* this is undefined if q - p > PTRDIFF_MAX */
		ptrdiff_t l = q - p;
		memcpy (r, p, l);
		r += l;
		memcpy (r, new, newlen);
		r += newlen;
	}
	strcpy (r, p);

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
	g_autoptr(GChecksum) cs;
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

	/* ensure nobody symlinked this directory to a different location */
	if (g_file_test (tmp_root_path, G_FILE_TEST_IS_SYMLINK))
		g_assert (g_unlink (tmp_root_path) == 0);

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

	/* ensure that the permissions on the root path haven't changed */
	g_chmod (tmp_root_path, 0777);

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
 * li_get_arch_triplet:
 *
 * Get the full architecture triplet, e.g. "x86_64-linux-gnu".
 *
 * Returns: (transfer full): The current arch triplet.
 */
gchar*
li_get_arch_triplet (void)
{
	gchar *triplet;
	gchar *tmp;
	struct utsname uts;

	uname (&uts);

	/* FIXME: We just assume the OS is GNU here - this needs to be detected better. */
	tmp = g_strdup_printf ("%s-%s-%s", uts.machine, uts.sysname, "gnu");
	triplet = g_ascii_strdown (tmp, -1);
	g_free (tmp);

	return triplet;
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

/**
 * li_parse_dependency_string:
 *
 * Parse a dependency string (e.g. "foo (>= 4.0)") and return a #LiPkgInfo
 * object reflecting the required package name and dependency relation.
 *
 * Returns: (transfer full): A new #LiPkgInfo reflecting the relation and package
 */
LiPkgInfo*
li_parse_dependency_string (const gchar *depstr)
{
	g_autofree gchar *dep_raw = NULL;
	LiPkgInfo *pki;

	dep_raw = g_strdup (depstr);
	g_strstrip (dep_raw);

	pki = li_pkg_info_new ();
	if (g_strrstr (dep_raw, "(") != NULL) {
		gchar **strv;
		gchar *ver_tmp;

		strv = g_strsplit (dep_raw, "(", 2);
		g_strstrip (strv[0]);

		li_pkg_info_set_name (pki, strv[0]);
		ver_tmp = strv[1];
		g_strstrip (ver_tmp);
		if (strlen (ver_tmp) > 2) {
			LiVersionFlags flags = LI_VERSION_UNKNOWN;
			guint i;

			/* extract the version relation (>>, >=, <=, ==, <<) */
			for (i = 0; i <= 1; i++) {
				if (ver_tmp[i] == '>')
					flags |= LI_VERSION_HIGHER;
				else if (ver_tmp[i] == '<')
					flags |= LI_VERSION_LOWER;
				else if (ver_tmp[i] == '=')
					flags |= LI_VERSION_EQUAL;
				else {
					g_warning ("Found invalid character in version relation: %c", ver_tmp[i]);
					flags = LI_VERSION_UNKNOWN;
				}
			}

			/* extract the version */
			if (g_str_has_suffix (ver_tmp, ")")) {
				ver_tmp = g_strndup (ver_tmp+2, strlen (ver_tmp)-3);
				g_strstrip (ver_tmp);

				li_pkg_info_set_version (pki, ver_tmp);
				li_pkg_info_set_version_relation (pki, flags);
				g_free (ver_tmp);
			} else {
				g_warning ("Malformed dependency string found: Closing bracket of version is missing: %s (%s", strv[0], ver_tmp);
			}
		}
		g_strfreev (strv);
	} else {
		li_pkg_info_set_name (pki, dep_raw);
	}

	return pki;
}

/**
 * li_parse_dependencies_string:
 *
 * Parse a dependencies string (in the form of "foo (>= 2.0), bar (== 1.0)") and return an array of #LiPkgInfo
 * objects reflecting the relations and packages specified in the dependency string.
 *
 * Returns: (transfer full) (element-type LiPkgInfo): A list of #LiPkgInfo objects reflecting the relation and package
 */
GPtrArray*
li_parse_dependencies_string (const gchar *depstr)
{
	guint i;
	gchar **slices;
	GPtrArray *array;

	if (depstr == NULL)
		return NULL;

	array = g_ptr_array_new_with_free_func (g_object_unref);
	slices = g_strsplit (depstr, ",", -1);

	for (i = 0; slices[i] != NULL; i++) {
		LiPkgInfo *pki;
		pki = li_parse_dependency_string (slices[i]);

		g_ptr_array_add (array, pki);
	}
	g_strfreev (slices);

	return array;
}

/**
 * li_env_get_user_fullname:
 *
 * Get the user's full name from the environment.
 *
 * Returns: User's full name, or %NULL on error. Free with g_free()
 */
gchar*
li_env_get_user_fullname (void)
{
	const gchar *var;

	var = g_getenv ("LIMBA_FULLNAME");
	if (var == NULL)
		var = g_getenv ("DEBFULLNAME");
	if (var == NULL)
		return NULL;
	else
		return g_strdup (var);
}

/**
 * li_env_get_user_email:
 *
 * Get the user's email address from the environment.
 *
 * Returns: Email address, or %NULL on error. Free with g_free()
 */
gchar*
li_env_get_user_email (void)
{
	const gchar *var;

	var = g_getenv ("LIMBA_EMAIL");
	if (var == NULL)
		var = g_getenv ("DEBEMAIL");
	if (var == NULL)
		return NULL;
	else
		return g_strdup (var);
}

/**
 * li_env_get_target_repo:
 *
 * Get the target repository name.
 *
 * Returns: Target repository name, or %NULL on error. Free with g_free()
 */
gchar*
li_env_get_target_repo (void)
{
	const gchar *var;
	var = g_getenv ("LIMBA_TARGET_REPO");
	if (var == NULL)
		return NULL;
	else
		return g_strdup (var);
}

/**
 * li_env_set_user_details:
 *
 * Set user details used when building a Limba package
 */
void
li_env_set_user_details (const gchar *user_name, const gchar *user_email, const gchar *target_repo)
{
	if (user_name != NULL)
		g_setenv ("LIMBA_FULLNAME", user_name, TRUE);
	if (user_email != NULL)
		g_setenv ("LIMBA_EMAIL", user_email, TRUE);
	if (target_repo != NULL)
		g_setenv ("LIMBA_TARGET_REPO", target_repo, TRUE);
}

/**
 * SdJobData:
 *
 * Helper data for systemd job callback
 */
struct SdJobData {
	gchar *job;
	GMainLoop *main_loop;
};

/**
 * li_sd_job_removed_cb:
 *
 * Helper callback
 */
static void
li_sd_job_removed_cb (LiSdManager *sdmgr, guint32 id, gchar *job, gchar *unit, gchar *result, struct SdJobData *data)
{
	if (strcmp (job, data->job) == 0)
		g_main_loop_quit (data->main_loop);
}

/**
 * li_add_to_new_scope:
 *
 * Add the current process to a new scope (cgroup).
 */
void
li_add_to_new_scope (const gchar *domain, const gchar *idname, GError **error)
{
	GDBusConnection *conn = NULL;
	LiSdManager *sdmgr = NULL;
	GMainLoop *main_loop = NULL;
	g_autofree gchar *sd_path = NULL;
	g_autofree gchar *sd_address = NULL;
	g_autofree gchar *sd_job = NULL;
	g_autofree gchar *cgname = NULL;
	GVariantBuilder builder;
	GVariant *properties = NULL;
	GVariant *aux = NULL;
	guint32 pid;
	GError *tmp_error = NULL;
	struct SdJobData data;

	if (domain == NULL)
		domain = "limba";

	/* check if systemd is running */
	if (li_utils_is_root ()) {
		sd_path = g_strdup ("/run/systemd/private");
	} else {
		sd_path = g_strdup_printf ("/run/user/%d/systemd/private", getuid ());
	}
	if (!g_file_test (sd_path, G_FILE_TEST_EXISTS))
		goto out;

	pid = getpid ();

	main_loop = g_main_loop_new (NULL, FALSE);

	sd_address = g_strconcat ("unix:path=", sd_path, NULL);
	conn = g_dbus_connection_new_for_address_sync (sd_address,
						G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
						NULL,
						NULL, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_prefixed_error (error, tmp_error,
					"Unable to connect to systemd.");
		goto out;
	}

	sdmgr = li_sd_manager_proxy_new_sync (conn,
					G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
					NULL,
					"/org/freedesktop/systemd1",
					NULL,
					&tmp_error);
	if (tmp_error != NULL) {
		g_propagate_prefixed_error (error, tmp_error,
					"Unable to create systemd manager proxy.");
		goto out;
	}

	cgname = g_strdup_printf ("%s-%s-%d.scope", domain, idname, pid);

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sv)"));
	g_variant_builder_add (&builder, "(sv)",
				"PIDs",
				g_variant_new_fixed_array (G_VARIANT_TYPE ("u"),
							&pid, 1, sizeof (guint32)));
	properties = g_variant_builder_end (&builder);

	aux = g_variant_new_array (G_VARIANT_TYPE ("(sa(sv))"), NULL, 0);
	li_sd_manager_call_start_transient_unit_sync (sdmgr,
						cgname, /* scope name */
						"fail", /* mode */
						properties,
						aux, /* unused, empty array */
						&sd_job,
						NULL,
						&tmp_error);
	if (tmp_error != NULL) {
		g_propagate_prefixed_error (error, tmp_error,
					"Unable to create systemd manager proxy.");
		goto out;
	}

	data.job = sd_job;
	data.main_loop = main_loop;
	g_signal_connect (sdmgr,
			"job-removed",
			G_CALLBACK (li_sd_job_removed_cb), &data);

	/* wait for the job to complete */
	g_main_loop_run (main_loop);

out:
	if (main_loop)
		g_main_loop_unref (main_loop);
	if (sdmgr)
		g_object_unref (sdmgr);
	if (conn)
		g_object_unref (conn);
}
