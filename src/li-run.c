/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2015 Matthias Klumpp <matthias@tenstral.net>
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

#define _GNU_SOURCE /* Required for CLONE_NEWNS */
#include <config.h>
#include "li-run.h"

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

/**
 * SECTION:li-run
 * @short_description: Primitive low-level functions to set up a runtime environment an run Limba apps.
 * This is a private module with non-stable API!
 * @include: limba.h
 */

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
 * li_run_acquire_caps:
 *
 * Ensure we have just the capabilities we need to set up the environment.
 */
gboolean
li_run_acquire_caps (void)
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
 * li_run_drop_caps:
 *
 * Drop all remaining capabilities we have.
 * This needs to be run before replacing the process tree
 * with the actual application (we don't want it to have privileges).
 */
gboolean
li_run_drop_caps (void)
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
 * li_run_env_setup:
 *
 * Setup a new environment to run the application in.
 * This function prints errors to stderr at time.
 *
 * Returns: A path to the new root filesystem, or %NULL on error.
 */
gchar*
li_run_env_setup (void)
{
	int res = 0;
	gchar *fname = NULL;
	uid_t uid;
	int mount_count;
	g_autofree gchar *newroot = NULL;
	g_autofree gchar *approot_dir = NULL;

	/* perform some preparation before we can mount the app */
	g_debug ("creating new namespace");
	res = unshare (CLONE_NEWNS);
	if (res != 0) {
		g_printerr ("Failed to create new namespace: %s\n", strerror(errno));
		return NULL;
	}

	/* Mark everything as slave, so that we still
	 * receive mounts from the real root, but don't
	 * propagate mounts to the real root. */
	if (mount (NULL, "/", NULL, MS_SLAVE | MS_REC, NULL) < 0) {
		g_printerr ("Failed to make / slave.\n");
		return NULL;
	}

	uid = getuid ();
	newroot = g_strdup_printf ("/run/user/%d/.limba-root", uid);
	if (g_mkdir_with_parents (newroot, 0755) != 0) {
		g_printerr ("Failed to create root tmpfs.\n");
		return NULL;
	}

	/* Create a tmpfs which we will use as / in the namespace */
	if (mount ("", newroot, "tmpfs", MS_NODEV | MS_NOEXEC | MS_NOSUID, NULL) != 0) {
		g_printerr ("Failed to mount tmpfs.\n");
		return NULL;
	}

	/* build & bindmount the root filesystem */
	if (mkdir_and_bindmount (newroot, "/bin", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/cdrom", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/dev", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/etc", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/home", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/lib", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/lib64", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/media", TRUE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/mnt", TRUE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/opt", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/proc", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/run", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/srv", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/sys", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/usr", FALSE) != 0)
		return NULL;
	if (mkdir_and_bindmount (newroot, "/var", TRUE) != 0)
		return NULL;

	fname = g_build_filename (newroot, "tmp", NULL);
	if (g_mkdir_with_parents (fname, 0755) != 0) {
		g_free (fname);
		g_printerr ("Unable to create root /tmp dir.\n");
		return NULL;
	}
	g_free (fname);

	fname = g_build_filename (newroot, ".oldroot", NULL);
	if (g_mkdir_with_parents (fname, 0755) != 0) {
		g_free (fname);
		g_printerr ("Unable to create root /.oldroot dir.\n");
		return NULL;
	}
	g_free (fname);

	/* the place where we will mount the application data to */
	approot_dir = g_build_filename (newroot, "app", NULL);
	if (g_mkdir_with_parents (approot_dir, 0755) != 0) {
		g_printerr ("Unable to create /app dir.\n");
		return NULL;
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
		goto error_out;
	}

	return g_strdup (newroot);

error_out:
	/* we have an error */
	while (mount_count-- > 0)
		umount (approot_dir);

	return NULL;
}

/**
 * li_run_env_enter:
 *
 * Enter (pivot_root) a previously prepared application environment.
 *
 * Returns: %TRUE on success.
 */
gboolean
li_run_env_enter (const gchar *newroot)
{
	/* now move into the application's private environment */
	chdir (newroot);
	if (pivot_root (newroot, ".oldroot") != 0) {
		g_printerr ("pivot_root failed: %s\n", strerror(errno));
		return FALSE;
	}
	chdir ("/");

	/* The old root better be rprivate or we will send unmount events to the parent namespace */
	if (mount (".oldroot", ".oldroot", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
		g_printerr ("Failed to make old root rprivate: %s\n", strerror (errno));
		return FALSE;
	}

	if (umount2 (".oldroot", MNT_DETACH)) {
		g_printerr ("unmount oldroot failed: %s\n", strerror (errno));
		return FALSE;
	}

	return TRUE;
}
