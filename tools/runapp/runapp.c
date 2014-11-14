/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2012 Alexander Larsson <alexl@redhat.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE /* Required for CLONE_NEWNS */
#include <config.h>
#include <limba.h>
#include <li-config-data.h>

#include <sys/mount.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/version.h>

#include <gio/gio.h>

#define GS_DEFINE_CLEANUP_FUNCTION(Type, name, func) \
  static inline void name (void *v) \
  { \
    func (*(Type*)v); \
  }
GS_DEFINE_CLEANUP_FUNCTION(void*, gs_local_free, g_free)
#define _cleanup_free_ __attribute__ ((cleanup(gs_local_free)))

/**
 * create_mount_namespace:
 */
static int
create_mount_namespace (void)
{
	int mount_count;
	int res;

	g_debug ("creating new namespace");

	res = unshare (CLONE_NEWNS);
	if (res != 0) {
		g_print ("Failed to create new namespace: %s\n", strerror(errno));
		return 1;
	}

	g_debug ("mount (private)");
	mount_count = 0;
	res = mount (LI_SW_ROOT_PREFIX, LI_SW_ROOT_PREFIX,
				 NULL, MS_PRIVATE, NULL);
	if (res != 0 && errno == EINVAL) {
		/* Maybe if failed because there is no mount
		 * to be made private at that point, lets
		 * add a bind mount there. */
		g_debug (("mount (bind)\n"));
		res = mount (LI_SW_ROOT_PREFIX, LI_SW_ROOT_PREFIX,
					 NULL, MS_BIND, NULL);
		/* And try again */
		if (res == 0) {
			mount_count++; /* Bind mount succeeded */
			g_debug ("mount (private)");
			res = mount (LI_SW_ROOT_PREFIX, LI_SW_ROOT_PREFIX,
						 NULL, MS_PRIVATE, NULL);
		}
	}

	if (res != 0) {
		g_error ("Failed to make prefix namespace private");
		goto error_out;
	}

	return 0;

error_out:
	while (mount_count-- > 0)
		umount (LI_SW_ROOT_PREFIX);
	return 1;
}

/**
 * mount_overlay:
 * @pkgid: A software identifier (name-version)
 */
static int
mount_overlay (const gchar *pkgid)
{
	int res = 0;
	gchar *main_data_path = NULL;
	gchar *fname = NULL;
	gchar *wdir = NULL;
	GFile *file;
	const gchar *runtime_uuid;
	gchar *tmp;
	LiPkgInfo *pki = NULL;

	/* check if the software exists */
	main_data_path = g_build_filename (LI_SOFTWARE_ROOT, pkgid, "data", NULL);
	fname = g_build_filename (LI_SOFTWARE_ROOT, pkgid, "control", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);

	if (!g_file_query_exists (file, NULL)) {
		fprintf (stderr, "The software '%s' does not exist.\n", pkgid);
		res = 1;
		g_object_unref (file);
		goto out;
	}

	pki = li_pkg_info_new ();
	li_pkg_info_load_file (pki, file);
	g_object_unref (file);

	runtime_uuid = li_pkg_info_get_runtime_dependency (pki);
	if (runtime_uuid == NULL) {
		fprintf (stderr, "Sorry, I can not construct a new runtime environment for this application. Please do that manually!\n");
		res = 3;
		goto out;
	}

	wdir = g_build_filename (LI_SOFTWARE_ROOT, "runtimes", "ofs_work", NULL);
	res = g_mkdir_with_parents (wdir, 0775);
	if (res != 0) {
		fprintf (stderr, "Unable to create OverlayFS workdir. %s\n", strerror (errno));
		res = 1;
		goto out;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION (3,18,0)
	g_warning ("Compiled for an old Linux kernel (<< 3.18). This tool might not work as expected.");
#endif

	if (g_strcmp0 (runtime_uuid, "None") != 0) {
		/* mount the desired runtime */
		gchar *rt_path;

		rt_path = g_build_filename (LI_SOFTWARE_ROOT, "runtimes", runtime_uuid, "data", NULL);
		if (!g_file_test (rt_path, G_FILE_TEST_IS_DIR)) {
			fprintf (stderr, "The runtime '%s' does not exist.\n", runtime_uuid);
			res = 1;
			g_free (rt_path);
			goto out;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,18,0)
		tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s,workdir=%s", LI_SW_ROOT_PREFIX, rt_path, wdir);
#else
		tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s", LI_SW_ROOT_PREFIX, rt_path);
#endif

		res = mount ("", LI_SW_ROOT_PREFIX,
					"overlayfs", MS_MGC_VAL | MS_RDONLY | MS_NOSUID, tmp);

		g_free (tmp);
		g_free (rt_path);
		if (res != 0) {
			fprintf (stderr, "Unable to mount runtime directory. %s\n", strerror (errno));
			res = 1;
			goto out;
		}
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION (3,18,0)
	tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s,workdir=%s", LI_SW_ROOT_PREFIX, main_data_path, wdir);
#else
	tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s", LI_SW_ROOT_PREFIX, main_data_path);
#endif

	res = mount ("", LI_SW_ROOT_PREFIX,
				 "overlayfs", MS_MGC_VAL | MS_RDONLY | MS_NOSUID, tmp);
	g_free (tmp);
	if (res != 0) {
		fprintf (stderr, "Unable to mount directory. %s\n", strerror (errno));
		res = 1;
		goto out;
	}

out:
	if (main_data_path != NULL)
		g_free (main_data_path);
	if (pki != NULL)
		g_object_unref (pki);
	if (wdir != NULL)
		g_free (wdir);

	return res;
}

/**
 * update_env_var_list:
 */
static void
update_env_var_list (const gchar *var, const gchar *item)
{
	const gchar *env;
	gchar *value;

	env = getenv (var);
	if (env == NULL || *env == 0) {
		setenv (var, item, 1);
	} else {
		value = g_strconcat (item, ":", env, NULL);
		setenv (var, value, 1);
		free (value);
	}
}

/**
 * main:
 */
int
main (gint argc, gchar *argv[])
{
	int ret;
	_cleanup_free_ gchar *swname = NULL;
	_cleanup_free_ gchar *executable = NULL;
	gchar **strv;
	gchar **child_argv = NULL;
	guint i;
	uid_t uid = getuid(), euid = geteuid();

	if ((uid > 0) && (uid == euid)) {
		g_error ("This program needs the suid bit to be set to function correpkiy.");
		return 3;
	}

	if (argc <= 1) {
		fprintf (stderr, "No application-id was specified.\n");
		return 1;
	}

	strv = g_strsplit (argv[1], ":", 2);
	if (g_strv_length (strv) != 2) {
		g_strfreev (strv);
		fprintf (stderr, "No installed software with that name or executable found.\n");
		ret = 1;
		goto out;
	}

	swname = g_strdup (strv[0]);
	executable = g_build_filename (LI_SW_ROOT_PREFIX, strv[1], NULL);
	g_strfreev (strv);

	ret = create_mount_namespace ();
	if (ret > 0)
		goto out;

	ret = mount_overlay (swname);
	if (ret > 0)
		goto out;

	/* Now we have everything we need CAP_SYS_ADMIN for, so drop setuid */
	setuid (getuid ());

	update_env_var_list ("LD_LIBRARY_PATH", LI_SW_ROOT_PREFIX "/lib");
	update_env_var_list ("LD_LIBRARY_PATH", LI_SW_ROOT_PREFIX "/usr/lib");

	child_argv = malloc ((1 + argc - 1) * sizeof (char *));
	if (child_argv == NULL) {
		ret = FALSE;
		fprintf (stderr, "Out of memory!\n");
		goto out;
	}

	i = 0;
	for (i = 0; i < argc; i++) {
		child_argv[i] = argv[i+1];
	}
	child_argv[i++] = NULL;

	return execv (executable, child_argv);

out:
	return ret;
}
