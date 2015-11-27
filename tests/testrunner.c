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

#include "config.h"
#include "testrunner.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>
#include <sys/utsname.h>
#include <glib/gi18n-lib.h>

#include "limba.h"
#include "li-utils-private.h"

#define TEST_ROOT "/var/tmp/limba-tests/root"
#define TEST_TMP "/var/tmp/limba-tests/volatile"

#define OFS_WDIR "/var/tmp/limba-tests/ofs_work"
#define li_assert_errno(e) if (e != 0) { g_print("ERROR: %s", strerror(errno)); g_assert (e == 0); }

static gboolean optn_show_version = FALSE;
static gboolean optn_verbose_mode = FALSE;

/**
 * ofsmount:
 *
 * Wrapper for OverlayFS mount.
 */
static void
ofsmount (const gchar *dir, const gchar *mlower, const gchar *mupper, unsigned long int options)
{
	gchar *mp;
	gint res;

	mp = g_strdup_printf ("lowerdir=%s,upperdir=%s,workdir=%s", mlower, mupper, OFS_WDIR);
	res = mount ("overlay", dir,
				"overlay", options, mp);
	li_assert_errno (res);
}

/**
 * ofsmount_sys:
 *
 * Mount a host system directory to the testsuite workspace.
 */
static void
ofsmount_sys (const gchar *dir, gboolean readonly)
{
	gchar *tmp_d;
	gchar *target_d;
	unsigned long int options;

	tmp_d = g_build_filename (TEST_TMP, dir, NULL);
	target_d = g_build_filename (TEST_ROOT, dir, NULL);

	options = MS_MGC_VAL | MS_NOSUID;
	if (readonly)
		options = options | MS_RDONLY;

	ofsmount (target_d, dir, tmp_d, options);

	g_free (tmp_d);
	g_free (target_d);
}

/**
 * ofsmount_tmp:
 *
 * Mount volatile, writeable test directory.
 */
static void
ofsmount_tmp (const gchar *dir)
{
	gchar *tmp_d;
	gchar *target_d;

	tmp_d = g_build_filename (TEST_TMP, dir, NULL);
	target_d = g_build_filename (TEST_ROOT, dir, NULL);

	ofsmount (target_d, target_d, tmp_d, MS_MGC_VAL | MS_NOSUID);

	g_free (tmp_d);
	g_free (target_d);
}

/**
 * test_env_mkdir:
 *
 * Create test-environment dir.
 */
void
test_env_mkdir (const gchar *dir)
{
	gchar *path;

	/* volatile */
	path = g_build_filename (TEST_TMP, dir, NULL);
	g_mkdir_with_parents (path, 0755);
	g_free (path);

	/* sys (non-writeable) */
	path = g_build_filename (TEST_ROOT, dir, NULL);
	g_mkdir_with_parents (path, 0755);
	g_free (path);
}

/**
 * li_test_finalize_chroot:
 *
 * Unmount everything.
 */
void
li_test_finalize_chroot ()
{
	gboolean ret;

	/* sys */
	umount (TEST_ROOT "/proc");
	umount (TEST_ROOT "/dev/pts");
	umount (TEST_ROOT "/dev");
	umount (TEST_ROOT "/usr");
	umount (TEST_ROOT "/etc");
	umount (TEST_ROOT "/lib");
	umount (TEST_ROOT "/home");
	umount (TEST_ROOT "/bin");
	umount (TEST_ROOT "/lib64");
	umount (TEST_ROOT "/lib32");
	umount (TEST_ROOT "/run");

	/* writeable */
	umount (TEST_ROOT "/var");
	umount (TEST_ROOT "/opt");
	umount (TEST_ROOT "/tmp");

	/* cleanup data from previous test runs */
	if (g_file_test (TEST_TMP, G_FILE_TEST_EXISTS)) {
		ret = li_delete_dir_recursive (TEST_TMP);
		g_assert (ret);
	}
}

/**
 * test_env_mount_devproc:
 */
static void
test_env_mount_devproc ()
{
	gint res;

	g_mkdir_with_parents (TEST_ROOT "/proc", 0755);
	g_mkdir_with_parents (TEST_ROOT "/dev/pts", 0755);

	res = mount ("devtmpfs", TEST_ROOT "/dev",
					"devtmpfs", MS_MGC_VAL, "");
	li_assert_errno (res);

	res = mount ("devpts", TEST_ROOT "/dev/pts",
					"devpts", MS_MGC_VAL, "");
	li_assert_errno (res);

	res = mount ("proc", TEST_ROOT "/proc",
					"proc", MS_MGC_VAL, "");
	li_assert_errno (res);
}

/**
 * li_test_enter_chroot:
 * @cleanup: %TRUE if a clean environment is required.
 *
 * Enter a chroot environment for Limba automatic tests.
 * These function make heavy use of assertions, and should only
 * be used with the testsuite.
 */
gboolean
li_test_enter_chroot ()
{
	gint res;
	struct utsname uts_data;

	/* we need to be root */
	if (!li_utils_is_root ())
		g_error ("This testsuite needs CAP_SYS_ADMIN to work.");

	uname (&uts_data);
	/* we need at least Linux 3.18 for the tests to work */
	if (li_compare_versions ("3.18", uts_data.release) > 0) {
		g_error ("Running on Linux %s. The testsuite needs at least Linux 3.18 to work properly.", uts_data.release);
		return FALSE;
	}

	/* unmount stuff */
	li_test_finalize_chroot ();

	g_mkdir_with_parents (OFS_WDIR, 0755);

	/* create directory structure */
	test_env_mkdir ("/usr");
	test_env_mkdir ("/lib");
	test_env_mkdir ("/etc/limba");
	test_env_mkdir ("/home");
	test_env_mkdir ("/tmp");
	test_env_mkdir ("/bin");
	test_env_mkdir ("/run");
	test_env_mkdir ("/var/lib");
	test_env_mkdir ("/var/cache");
	test_env_mkdir ("/opt/software");
	test_env_mkdir ("/app");

	/* create installer locations */
	test_env_mkdir ("/usr/local/bin");
	test_env_mkdir ("/usr/local/share/applications");

	/* populate /dev */
	test_env_mount_devproc ();

	/* mount some host system dirs, most of them non-writeable */
	ofsmount_sys ("/usr", FALSE); /* installer writes into /usr */
	ofsmount_sys ("/etc", FALSE);
	ofsmount_sys ("/lib", TRUE);
	ofsmount_sys ("/home", TRUE);
	ofsmount_sys ("/bin", TRUE);
	ofsmount_sys ("/run", TRUE); /* sometimes needed for network access - we can mount in non-writeable here */

	/* mount dirs with writeable overlay */
	ofsmount_tmp ("/var");
	ofsmount_tmp ("/opt");
	ofsmount_tmp ("/tmp");

	/* mount special libdirs, if we have them - distributions vary here */
	if (g_file_test ("/lib64", G_FILE_TEST_EXISTS)) {
		test_env_mkdir ("/lib64");
		ofsmount_sys ("/lib64", TRUE);
	}
	if (g_file_test ("/lib32", G_FILE_TEST_EXISTS)) {
		test_env_mkdir ("/lib32");
		ofsmount_sys ("/lib32", TRUE);
	}

	/* change root */
	res = chroot (TEST_ROOT);
	chdir ("/");

	g_assert (res == 0);

	return TRUE;
}

/**
 * li_test_drop_privileges:
 */
void
li_test_drop_privileges ()
{
	if (li_utils_is_root ())
		setuid (getuid ());
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gint exit_code = 0;
	gboolean ret;
	gchar *exec_cmd;
	GOptionContext *opt_context;
	GError *error = NULL;

	const GOptionEntry client_options[] = {
		{ "version", 0, 0, G_OPTION_ARG_NONE, &optn_show_version, _("Show the program version"), NULL },
		{ "verbose", (gchar) 0, 0, G_OPTION_ARG_NONE, &optn_verbose_mode, _("Show extra debugging information"), NULL },
		{ NULL }
	};

	opt_context = g_option_context_new ("- Limba Privileged Test Runner");
	g_option_context_set_help_enabled (opt_context, TRUE);
	g_option_context_add_main_entries (opt_context, client_options, NULL);

	g_option_context_parse (opt_context, &argc, &argv, &error);
	if (error != NULL) {
		gchar *msg;
		msg = g_strconcat (error->message, "\n", NULL);
		g_print ("%s\n", msg);
		g_free (msg);
		g_error (_("Run '%s --help' to see a full list of available command line options."), argv[0]);
		exit_code = 1;
		g_error_free (error);
		goto out;
	}

	if (optn_show_version) {
		g_print ("Limba version: %s\n", VERSION);
		goto out;
	}

	/* just a hack, we might need proper message handling later */
	if (optn_verbose_mode) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}

	if (argc < 2) {
		g_printerr ("%s\n", _("You need to specify a command."));
		exit_code = 1;
		goto out;
	}

	/* switch to fake root environment */
	ret = li_test_enter_chroot ();
	g_assert (ret);

	exec_cmd = g_strjoinv (" ", argv+1);
	g_debug ("Running: %s", exec_cmd);

	exit_code = system (exec_cmd);
	g_free (exec_cmd);

out:
	g_option_context_free (opt_context);

	return exit_code;
}
