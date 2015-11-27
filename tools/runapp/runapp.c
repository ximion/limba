/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2012-2015 Alexander Larsson <alexl@redhat.com>
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
#include "li-utils-private.h"

#include <sys/mount.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/unistd.h>
#include <linux/version.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/capability.h>
#include <sys/prctl.h>

#include <gio/gio.h>

#define REQUIRED_CAPS (CAP_TO_MASK(CAP_SYS_ADMIN))

typedef enum {
  BIND_READONLY = (1<<0),
  BIND_PRIVATE = (1<<1),
  BIND_DEVICES = (1<<2),
} bind_option_t;

/**
 * pivot_root:
 *
 * Change the root filesystem.
 */
static int
pivot_root (const char * new_root, const char * put_old)
{
#ifdef __NR_pivot_root
  return syscall(__NR_pivot_root, new_root, put_old);
#else
  errno = ENOSYS;
  return -1;
#endif
}

/**
 * Ensure we have just the capabilities we need
 */
static gboolean
acquire_caps (void)
{
	struct __user_cap_header_struct hdr;
	struct __user_cap_data_struct data;

	if (getuid () != geteuid ()) {
		/* Tell kernel not clear capabilities when dropping root */
		if (prctl (PR_SET_KEEPCAPS, 1, 0, 0, 0) < 0) {
			g_printerr ("prctl(PR_SET_KEEPCAPS) failed\n");
			return FALSE;
		}

		/* Drop root uid, but retain the required permitted caps */
		if (setuid (getuid ()) < 0) {
			g_printerr ("unable to drop privileges\n");
			return FALSE;
		}
	}

	memset (&hdr, 0, sizeof(hdr));
	hdr.version = _LINUX_CAPABILITY_VERSION;

	/* Drop all non-require capabilities */
	data.effective = REQUIRED_CAPS;
	data.permitted = REQUIRED_CAPS;
	data.inheritable = 0;
	if (capset (&hdr, &data) < 0) {
		g_printerr ("capset failed\n");
		return FALSE;
	}

	/* Never gain any more privs during exec */
	if (prctl (PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
		g_printerr ("prctl(PR_SET_NO_NEW_CAPS) failed");
		return FALSE;
	}

	return TRUE;
}

/**
 * drop_caps:
 *
 * Drop all remaining capabilities we have.
 */
static gboolean
drop_caps (void)
{
	struct __user_cap_header_struct hdr;
	struct __user_cap_data_struct data;

	memset (&hdr, 0, sizeof(hdr));
	hdr.version = _LINUX_CAPABILITY_VERSION;
	data.effective = 0;
	data.permitted = 0;
	data.inheritable = 0;

	if (capset (&hdr, &data) < 0) {
		g_printerr ("capset failed\n");
		return FALSE;
	}

	return TRUE;
}

/**
 * bind_mount:
 */
static int
bind_mount (const gchar *src, const gchar *dest, bind_option_t options)
{
	gboolean readonly = (options & BIND_READONLY) != 0;
	gboolean private = (options & BIND_PRIVATE) != 0;

	if (mount (src, dest, NULL, MS_MGC_VAL | MS_BIND | (readonly?MS_RDONLY:0), NULL) != 0)
		return 1;

	if (private) {
		if (mount ("none", dest, NULL, MS_REC | MS_PRIVATE, NULL) != 0)
			return 2;
	}

	return 0;
}

/**
 * mkdir_and_bindmount:
 *
 * Create directory and place a bindmount or symlink (depending on the situation).
 */
static int
mkdir_and_bindmount (const gchar *root, const gchar *target, gboolean writable)
{
	struct stat buf;
	int res;
	g_autofree gchar *dir = NULL;

	if (!g_file_test (target, G_FILE_TEST_EXISTS))
		return 0;

	dir = g_build_filename (root, target, NULL);

	lstat (target, &buf);
	if ((buf.st_mode & S_IFMT) == S_IFLNK) {
		/* we have a symbolic link */

		if (symlink (dir, target) != 0) {
			g_printerr ("Symlink failed (%s).\n", target);
			return 1;
		}
	} else {
		/* we have a regular file */
		if (g_mkdir_with_parents (dir, 0755) != 0) {
			g_printerr ("Unable to create %s.\n", target);
			return 1;
		}

		res = bind_mount (target, dir, BIND_PRIVATE | (writable?0:BIND_READONLY));
		if (res != 0) {
			g_printerr ("Bindmount failed (%i).\n", res);
			return 1;
		}
	}

	return 0;
}

/**
 * mount_app_bundle:
 * @pkgid: A software identifier ("name/version")
 */
static int
mount_app_bundle (const gchar *pkgid)
{
	int res = 0;
	int mount_count;
	gchar *main_data_path = NULL;
	gchar *fname = NULL;
	GFile *file;
	const gchar *runtime_uuid;
	gchar *tmp;
	GString *lowerdirs = NULL;
	LiPkgInfo *pki = NULL;
	uid_t uid;
	g_autofree gchar *newroot = NULL;
	g_autofree gchar *approot_dir = NULL;
	GError *error = NULL;

	/* perform some preparation before we can mount the app */
	g_debug ("creating new namespace");
	res = unshare (CLONE_NEWNS);
	if (res != 0) {
		g_printerr ("Failed to create new namespace: %s\n", strerror(errno));
		return 1;
	}

	/* Mark everything as slave, so that we still
	 * receive mounts from the real root, but don't
	 * propagate mounts to the real root. */
	if (mount (NULL, "/", NULL, MS_SLAVE | MS_REC, NULL) < 0) {
		g_printerr ("Failed to make / slave.\n");
		return 1;
	}

	uid = getuid ();
	newroot = g_strdup_printf ("/run/user/%d/.limba-root", uid);
	if (g_mkdir_with_parents (newroot, 0755) != 0) {
		g_printerr ("Failed to create root tmpfs.\n");
		return 1;
	}

	/* Create a tmpfs which we will use as / in the namespace */
	if (mount ("", newroot, "tmpfs", MS_NODEV | MS_NOEXEC | MS_NOSUID, NULL) != 0)
		g_printerr ("Failed to mount tmpfs.\n");

	/* build & bindmount the root filesystem */
	if (mkdir_and_bindmount (newroot, "/bin", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/cdrom", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/dev", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/etc", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/home", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/lib", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/lib64", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/media", TRUE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/mnt", TRUE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/opt", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/proc", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/run", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/srv", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/sys", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/usr", FALSE) != 0)
		return 2;
	if (mkdir_and_bindmount (newroot, "/var", TRUE) != 0)
		return 2;

	fname = g_build_filename (newroot, "tmp", NULL);
	if (g_mkdir_with_parents (fname, 0755) != 0) {
		g_free (fname);
		g_printerr ("Unable to create root /tmp dir.\n");
		return 1;
	}
	g_free (fname);

	fname = g_build_filename (newroot, ".oldroot", NULL);
	if (g_mkdir_with_parents (fname, 0755) != 0) {
		g_free (fname);
		g_printerr ("Unable to create root /.oldroot dir.\n");
		return 1;
	}
	g_free (fname);

	/* the place where we will mount the application data to */
	approot_dir = g_build_filename (newroot, "app", NULL);
	if (g_mkdir_with_parents (approot_dir, 0755) != 0) {
		g_printerr ("Unable to create /app dir.\n");
		return 1;
	}

	g_debug ("mount (private)");
	mount_count = 0;
	res = mount (approot_dir, approot_dir,
				 NULL, MS_PRIVATE, NULL);
	if (res != 0 && errno == EINVAL) {
		/* Maybe if failed because there is no mount
		 * to be made private at that point, lets
		 * add a bind mount there. */
		g_debug (("mount (bind)\n"));
		res = mount (approot_dir, approot_dir,
					 NULL, MS_BIND, NULL);
		/* And try again */
		if (res == 0) {
			mount_count++; /* Bind mount succeeded */
			g_debug ("mount (private)");
			res = mount (approot_dir, approot_dir,
						 NULL, MS_PRIVATE, NULL);
		}
	}

	if (res != 0) {
		g_error ("Failed to make prefix namespace private");
		res = 1;
		goto out;
	}


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
	li_pkg_info_load_file (pki, file, &error);
	g_object_unref (file);
	if (error != NULL) {
		fprintf (stderr, "Unable to read software metadata. %s\n", error->message);
		goto out;
	}

	runtime_uuid = li_pkg_info_get_runtime_dependency (pki);
	if (runtime_uuid == NULL) {
		fprintf (stderr, "Sorry, I can not construct a new runtime environment for this application. Please do that manually!\n");
		res = 3;
		goto out;
	}

	lowerdirs = g_string_new ("");
	if (g_strcmp0 (runtime_uuid, "None") != 0) {
		/* mount the desired runtime */
		g_autoptr(LiRuntime) rt = NULL;
		gchar **rt_members;
		guint i;

		rt = li_runtime_new ();
		li_runtime_load_by_uuid (rt, runtime_uuid, &error);
		if (error != NULL) {
			fprintf (stderr, "Unable to load runtime '%s': %s\n", runtime_uuid, error->message);
			res = 1;
			goto out;
		}

		rt_members = (gchar**) g_hash_table_get_keys_as_array (li_runtime_get_members (rt), NULL);

		/* build our lowerdir directive */
		for (i = 0; rt_members[i] != NULL; i++) {
			g_string_append_printf (lowerdirs, "%s/%s/data:", LI_SOFTWARE_ROOT, rt_members[i]);
		}

		/* cleanup */
		g_free (rt_members);
	}

	/* append main data path */
	g_string_append_printf (lowerdirs, "%s:", main_data_path);

	/* safeguard againt the case where only one path is set for lowerdir.
	 * OFS doesn't like that, so we always set the root path as source too.
	 * This also terminates the lowerdir parameter. */
	g_string_append_printf (lowerdirs, "%s", approot_dir);

	tmp = g_strdup_printf ("lowerdir=%s", lowerdirs->str);
	res = mount ("overlay", approot_dir,
				 "overlay", MS_MGC_VAL | MS_RDONLY | MS_NOSUID, tmp);
	g_free (tmp);
	if (res != 0) {
		fprintf (stderr, "Unable to mount directory. %s\n", strerror (errno));
		res = 1;
		goto out;
	}

	/* now move into the application's private environment */
	chdir (newroot);
	if (pivot_root (newroot, ".oldroot") != 0) {
		g_printerr ("pivot_root failed: %s\n", strerror(errno));
		res = 2;
		goto out;
	}
	chdir ("/");

	/* The old root better be rprivate or we will send unmount events to the parent namespace */
	if (mount (".oldroot", ".oldroot", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
		g_printerr ("Failed to make old root rprivate: %s\n", strerror(errno));
		res = 2;
		goto out;
	}

	if (umount2 (".oldroot", MNT_DETACH)) {
		g_printerr ("unmount oldroot failed: %s\n", strerror(errno));
		res = 2;
		goto out;
	}

out:
	if (lowerdirs != NULL)
		g_string_free (lowerdirs, TRUE);
	if (main_data_path != NULL)
		g_free (main_data_path);
	if (pki != NULL)
		g_object_unref (pki);
	if (error != NULL)
		g_error_free (error);

	if (res != 0) {
		/* we have an error */
		while (mount_count-- > 0)
			umount (approot_dir);
	}

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
	g_autofree gchar *swname = NULL;
	g_autofree gchar *scope_name = NULL;
	g_autofree gchar *executable = NULL;
	struct utsname uts_data;
	gchar *ma_lib_path = NULL;
	gchar *tmp;
	g_auto(GStrv) strv = NULL;
	gchar **child_argv = NULL;
	guint i;
	GError *error = NULL;

	/* ensue we have required capabilities, and drop all the ones we don't need */
	if (!acquire_caps ()) {
		g_printerr ("This program needs the suid bit to be set to function correctly.\n");
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
		goto error;
	}

	uname (&uts_data);
	/* we need at least Linux 4.0 for Limba to work properly */
	if (li_compare_versions ("4.0", uts_data.release) > 0)
		g_warning ("Running on Linux %s. Runapp needs at least Linux 4.0 to be sure all needed features are present.", uts_data.release);

	/* get the bundle name */
	swname = g_strdup (strv[0]);

	/* create our environment */
	ret = mount_app_bundle (swname);
	if (ret > 0)
		goto error;

	/* Now we have everything we need CAP_SYS_ADMIN for, so drop that capability */
	if (!drop_caps ()) {
		g_printerr ("Unable to drop capabilities.\n");
		ret = 3;
		goto error;
	}

	/* place this process in a new cgroup */
	scope_name = li_str_replace (swname, "/", "");
	li_add_to_new_scope ("app", scope_name, &error);
	if (error != NULL) {
		fprintf (stderr, "Could not add process to new scope. %s\n", error->message);
		g_error_free (error);
		goto error;
	}

	/* determine which command we should execute */
	if (g_strcmp0 (strv[1], "sh") == 0) {
		/* don't run an application, get a interactive shell instead */
		executable = g_strdup ("/bin/sh");
		chdir (LI_SW_ROOT_PREFIX);
	} else {
		executable = g_build_filename (LI_SW_ROOT_PREFIX, strv[1], NULL);
	}

	/* add generic library path */
	update_env_var_list ("LD_LIBRARY_PATH", LI_SW_ROOT_PREFIX "/lib");

	/* add multiarch library path for compatibility reasons */
	tmp = li_get_arch_triplet ();
	ma_lib_path = g_build_filename (LI_SW_ROOT_PREFIX, "lib", tmp, NULL);
	g_free (tmp);
	update_env_var_list ("LD_LIBRARY_PATH", ma_lib_path);
	g_free (ma_lib_path);

	/* add generic binary directory to PATH */
	update_env_var_list ("PATH", LI_SW_ROOT_PREFIX "/bin");

	child_argv = malloc ((argc) * sizeof (char *));
	if (child_argv == NULL) {
		ret = FALSE;
		fprintf (stderr, "Out of memory!\n");
		goto error;
	}

	if (!g_file_test (executable, G_FILE_TEST_EXISTS)) {
		ret = FALSE;
		fprintf (stderr, "Executable '%s' was not found.\n", executable);
		goto error;
	}

	/* give absolute executable path as argv[0] */
	child_argv[0] = executable;

	for (i = 1; i < argc - 1; i++) {
		child_argv[i] = argv[i+1];
	}
	child_argv[i++] = NULL;

	return execv (executable, child_argv);

error:
	return ret;
}
