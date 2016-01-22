/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015-2016 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * SECTION:li-build-master
 * @short_description: Coordinate an run a package build process.
 */

#define _GNU_SOURCE

#include "li-build-master.h"

#include <config.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sched.h>
#include <grp.h>
#include <pwd.h>

#include "li-utils.h"
#include "li-utils-private.h"
#include "li-run.h"
#include "li-pkg-info.h"
#include "li-package-graph.h"
#include "li-manager.h"
#include "li-build-conf.h"

typedef struct _LiBuildMasterPrivate	LiBuildMasterPrivate;
struct _LiBuildMasterPrivate
{
	gchar *build_root;
	gboolean init_done;

	gchar *chroot_orig_dir;

	GPtrArray *cmds_pre;
	GPtrArray *cmds;
	GPtrArray *cmds_post;
	LiPkgInfo *pki;
	gboolean ignore_foundations;

	gchar **dep_data_paths;

	gchar *username;
	gchar *email;
	gchar *target_repo;

	gboolean get_shell;

	uid_t build_uid;
	gid_t build_gid;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiBuildMaster, li_build_master, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_build_master_get_instance_private (o))

/**
 * li_build_master_finalize:
 **/
static void
li_build_master_finalize (GObject *object)
{
	LiBuildMaster *bmaster = LI_BUILD_MASTER (object);
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	g_free (priv->build_root);
	g_free (priv->chroot_orig_dir);
	if (priv->cmds_pre != NULL)
		g_ptr_array_unref (priv->cmds_pre);
	if (priv->cmds != NULL)
		g_ptr_array_unref (priv->cmds);
	if (priv->cmds_post != NULL)
		g_ptr_array_unref (priv->cmds_post);
	if (priv->dep_data_paths != NULL)
		g_strfreev (priv->dep_data_paths);
	if (priv->pki != NULL)
		g_object_unref (priv->pki);
	g_free (priv->email);
	g_free (priv->username);
	g_free (priv->target_repo);

	G_OBJECT_CLASS (li_build_master_parent_class)->finalize (object);
}

/**
 * li_build_master_init:
 **/
static void
li_build_master_init (LiBuildMaster *bmaster)
{
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	priv->build_root = NULL;
	priv->dep_data_paths = NULL;
	priv->get_shell = FALSE;

	priv->build_uid = getuid ();
	priv->build_gid = getgid ();
	priv->pki = NULL;
	priv->ignore_foundations = FALSE;
}

/**
 * li_build_master_check_dependencies:
 */
static void
li_build_master_check_dependencies (LiPackageGraph *pg, LiManager *mgr, LiPkgInfo *pki, gboolean use_builddeps, GError **error)
{
	g_autoptr(GPtrArray) all_pkgs = NULL;
	GError *tmp_error = NULL;
	g_autoptr(GPtrArray) deps = NULL;
	g_autoptr(GPtrArray) install_todo = NULL;
	guint i;

	if (use_builddeps) {
		/* we need to take the build-deps from the package we want to build... */
		deps = li_parse_dependencies_string (li_pkg_info_get_build_dependencies (pki));
	} else {
		/* and the regular deps from any other pkg */
		deps = li_parse_dependencies_string (li_pkg_info_get_dependencies (pki));
	}

	/* do we have dependencies at all? */
	if (deps == NULL)
		return;

	all_pkgs = li_manager_get_software_list (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	install_todo = g_ptr_array_new ();
	for (i = 0; i < deps->len; i++) {
		LiPkgInfo *ipki;
		gboolean ret;
		LiPkgInfo *dep = LI_PKG_INFO (g_ptr_array_index (deps, i));

		/* test if we have a dependency on a system component */
		ret = li_package_graph_test_foundation_dependency (pg, dep, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}
		/* continue if dependency is already satisfied */
		if (ret)
			continue;

		/* test if this package is already in the installed set */
		ipki = li_find_satisfying_pkg (all_pkgs, dep);
		if (ipki == NULL) {
			/* no installed package found that satisfies our requirements */
			g_set_error (error,
					LI_BUILD_MASTER_ERROR,
					LI_BUILD_MASTER_ERROR_BUILD_DEP_MISSING,
					_("Could not find bundle '%s' which is necessary to build this software."),
					li_pkg_info_get_name (dep));
		} else if (li_pkg_info_has_flag (ipki, LI_PACKAGE_FLAG_INSTALLED)) {
			/* dependency is already installed, add it as satisfied */
			li_package_graph_add_package (pg, pki, ipki, dep);

			/* we need a full dependency tree */
			li_build_master_check_dependencies (pg, mgr, ipki, FALSE, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return;
			}
		} else {
			g_ptr_array_add (install_todo, dep);
			continue;
		}
	}

	if (install_todo->len > 0) {
		g_autoptr(GString) depline = NULL;

		depline = g_string_new ("");
		for (i = 0; i < install_todo->len; i++) {
			LiPkgInfo *dep = LI_PKG_INFO (g_ptr_array_index (install_todo, i));
			g_string_append_printf (depline, "%s ", li_pkg_info_get_id (dep));
		}
		if (depline->len > 0)
			g_string_truncate (depline, depline->len - 1);

		g_set_error (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_BUILD_DEP_MISSING,
				_("Bundle(s) '%s' need to be installed in order to build this software."),
				depline->str);
		return;
	}
}

/**
 * li_build_master_resolve_builddeps:
 */
static void
li_build_master_resolve_builddeps (LiBuildMaster *bmaster, GError **error)
{
	g_autoptr(LiPackageGraph) pg = NULL;
	g_autoptr(GPtrArray) full_deps = NULL;
	g_autoptr(GHashTable) depdirs = NULL;
	g_autoptr(LiManager) mgr = NULL;
	guint i;
	GError *tmp_error = NULL;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	pg = li_package_graph_new ();
	li_package_graph_set_ignore_foundations (pg, priv->ignore_foundations);

	/* ensure the graph is initialized and additional data (foundations list) is loaded */
	li_package_graph_initialize (pg, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	mgr = li_manager_new ();
	li_package_graph_add_package (pg, NULL, priv->pki, NULL);

	li_build_master_check_dependencies (pg, mgr, priv->pki, TRUE, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	full_deps = li_package_graph_branch_to_array (pg, priv->pki, FALSE);
	if (full_deps == NULL) {
		g_warning ("Building package with no build-dependencies defined.");
		return;
	}

	depdirs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (i = 0; i < full_deps->len; i++) {
		const gchar *pkid;
		g_autofree gchar *datapath = NULL;
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (full_deps, i));
		pkid = li_pkg_info_get_id (pki);

		/* filter system dependencies */
		if (g_str_has_prefix (li_pkg_info_get_name (pki), "foundation:")) {
			continue;
		}

		datapath = g_build_filename (LI_SOFTWARE_ROOT, pkid, "data", NULL);
		g_hash_table_add (depdirs, g_strdup (datapath));
	}

	if (priv->dep_data_paths != NULL)
		g_strfreev (priv->dep_data_paths);

	priv->dep_data_paths = (gchar**) g_hash_table_get_keys_as_array (depdirs, NULL);
	g_hash_table_steal_all (depdirs);
}

/**
 * li_build_master_init_build:
 */
void
li_build_master_init_build (LiBuildMaster *bmaster, const gchar *dir, const gchar *chroot_orig, GError **error)
{
	g_autoptr(LiBuildConf) bconf = NULL;
	GError *tmp_error = NULL;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	if (chroot_orig != NULL)
		priv->chroot_orig_dir = g_strdup (chroot_orig);
	else
		priv->chroot_orig_dir = g_strdup ("/"); /* we take the normal root dir insread of a clean environment */

	if (priv->init_done) {
		g_set_error_literal (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_FAILED,
				"Tried to initialize the build-master twice. This is a bug in the application.");
		return;
	}

	bconf = li_build_conf_new ();
	li_build_conf_open_from_dir (bconf, dir, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	priv->cmds = li_build_conf_get_script (bconf);
	if (priv->cmds == NULL) {
		g_set_error_literal (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_NO_COMMANDS,
				_("Could not find commands to build this application!"));
		return;
	}

	priv->cmds_pre = li_build_conf_get_before_script (bconf);
	priv->cmds_post = li_build_conf_get_after_script (bconf);

	priv->build_root = g_strdup (dir);

	/* get list of build dependencies */
	if (priv->pki != NULL)
		g_object_unref (priv->pki);
	priv->pki = li_build_conf_get_pkginfo (bconf);
	li_build_master_resolve_builddeps (bmaster, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	priv->init_done = TRUE;
}

/**
 * li_build_master_print_section:
 */
static void
li_build_master_print_section (LiBuildMaster *bmaster, const gchar *section_name)
{
	GString *str;
	gint seclen;
	gint i;

	seclen = strlen (section_name);

	str = g_string_new ("\n");

	g_string_append_unichar (str, 0x250C);
	for (i = 0; i < seclen+14; i++)
		g_string_append_unichar (str, 0x2500);
	g_string_append_unichar (str, 0x2510);

	g_string_append_printf (str,
				"\n│ %s             │\n",
				section_name);

	g_string_append_unichar (str, 0x2514);
	for (i = 0; i < seclen+14; i++)
		g_string_append_unichar (str, 0x2500);
	g_string_append_unichar (str, 0x2518);
	g_string_append (str, "\n\n");

	printf ("%s", str->str);
	g_string_free (str, TRUE);

	fflush (stdout);
}

#if 0
/**
 * li_build_master_exec:
 */
static gint
li_build_master_exec (LiBuildMaster *bmaster, const gchar *cmd)
{
	gint res = 0;

	g_print (" ! %s\n", cmd);
	res = system (cmd);
	if (res > 255)
		res = 1;

	return res;
}
#endif

/**
 * li_build_master_exec_command_sequence:
 *
 */
static gint
li_build_master_exec_command_sequence (LiBuildMaster *bmaster, const gchar *stage_id, GPtrArray *cmds, const gchar *env_fname)
{
	gint fd;
	guint i;
	g_autofree gchar *tmp_fname = NULL;
	g_autofree gchar *scmd = NULL;
	gint res = 0;

	/* The system() command always spawns a new shell, so if you set
	 * environment vars or change directories in build.yml it won't work.
	 * So we cheat and execute a shell script instead, which even sources
	 * the environment from previous steps.
	 * This is more hack than a proper solution, so if you have a better idea
	 * for this, please implement it. */

	tmp_fname = g_strdup_printf ("/tmp/%s-XXXXXX", stage_id);
	fd = mkstemp (tmp_fname);
	g_debug ("Using command script: %s", tmp_fname);
	if (fd < 0) {
		g_error ("Unable to store script for %s.", stage_id);
		return 1;
	}

	res = g_chmod (tmp_fname, 0775);
	if (res < 0) {
		g_error ("Unable to set permissions: %s", g_strerror (errno));
		return res;
	}

	if (dprintf (fd, "#!/bin/sh\n. %s\nset -e\n\n", env_fname) < 0) {
		g_error ("Unable to write command sequence.");
		return 1;
	}

	for (i = 0; i < cmds->len; i++) {
		gchar *cmd;
		gchar *tmp;
		g_autofree gchar *msg = NULL;
		cmd = (gchar*) g_ptr_array_index (cmds, i);

		tmp = g_shell_quote (cmd);
		msg = li_str_replace (tmp, "'", "");
		g_free (tmp);

		if (dprintf (fd, "echo ' ! %s'\n", msg) < 0) {
			g_error ("Unable to write command sequence.");
			return 1;
		}
		if (dprintf (fd, "%s\n", cmd) < 0) {
			g_error ("Unable to write command sequence.");
			return 1;
		}
		dprintf (fd, "\n");
	}

	/* ensure we export the environment to our environment file */
	if (dprintf (fd, "export > %s\n", env_fname) < 0) {
		g_error ("Unable to write command sequence.");
		return 1;
	}

	/* run command script */
	scmd = g_strdup_printf ("sh %s", tmp_fname);
	res = system (scmd);
	if (res > 255)
		res = 1;
	close (fd);

	return res;
}

/**
 * li_build_master_mount_deps:
 *
 * Mount the depdenencies into the environment as an overlay.
 */
static gint
li_build_master_mount_deps (LiBuildMaster *bmaster, const gchar *chroot_dir, const gchar *env_root)
{
	guint i;
	gint res;
	g_autoptr(GString) lowerdirs = NULL;
	g_autofree gchar *mount_target = NULL;
	g_autofree gchar *volatile_data_dir = NULL;
	g_autofree gchar *ofs_wdir = NULL;
	g_autofree gchar *app_mount_target = NULL;
	gchar *tmp;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	if ((priv->dep_data_paths == NULL) || (priv->dep_data_paths[0] == NULL))
		return 0;

	/* The payload of Limba bundles follows a strict directory layout, with directories
	 * like bin/, share/, include/, lib/, etc. being at the toplevel.
	 * This means we can simply mount all payload over /usr here, and bindmount
	 * /app to /usr to make things work at build-time.
	 */
	mount_target = g_build_filename (chroot_dir, "usr", NULL);

	g_debug ("Mounting build dependencies into environment.");
	lowerdirs = g_string_new ("");
	for (i = 0; priv->dep_data_paths[i] != NULL; i++) {
		const gchar *dep_data_path = priv->dep_data_paths[i];
		g_string_append_printf (lowerdirs, "%s:", dep_data_path);
	}
	g_string_append (lowerdirs, mount_target);

	/* IMPORTANT: We do *not* mount the root filesystem NOSUID, so the build process can use sudo on demand.
	 * All other stuff and especially the environment when running the app *must not* perform mounts with SUID allowed. */
	tmp = g_strdup_printf ("lowerdir=%s", lowerdirs->str);
	res = mount ("overlay", mount_target,
				 "overlay", MS_MGC_VAL | MS_RDONLY, tmp);
	g_free (tmp);
	if (res != 0) {
		fprintf (stderr, "Unable to mount directory. %s\n", strerror (errno));
		res = 1;
		goto out;
	}

	/* Now we mount /app as an overlay on /usr and make it writable using OverlayFS.
	 * That way, binaries compiled with that prefix can find their data and we allow
	 * the setup process to place data here which can then be found by other steps in
	 * the same stup process. This is useful for creating one bundle out of multiple
	 * submodules. */

	/* create volatile data dir for /app */
	volatile_data_dir = g_build_filename (env_root, "volatile_app", NULL);
	res = g_mkdir_with_parents (volatile_data_dir, 0755);
	if (res != 0) {
		g_warning ("Unable to set up the environment (volatile /app): %s", g_strerror (errno));
		goto out;
	}

	/* create OverlayFS work dir */
	ofs_wdir = g_build_filename (env_root, "ofs_work_app", NULL);
	res = g_mkdir_with_parents (ofs_wdir, 0755);
	if (res != 0) {
		g_warning ("Unable to set up the environment (workdir /app): %s", g_strerror (errno));
		goto out;
	}

	/* now mount our build-data directory via OverlayFS */
	tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s,workdir=%s", mount_target, volatile_data_dir, ofs_wdir);
	app_mount_target = g_build_filename (chroot_dir, LI_SW_ROOT_PREFIX, NULL);
	res = mount ("overlay", app_mount_target,
			 "overlay", MS_MGC_VAL | MS_NOSUID, tmp);
	g_free (tmp);
	if (res != 0) {
		g_warning ("Unable to set up the environment (/app mount): %s", g_strerror (errno));
		goto out;
	}

	res = chown (volatile_data_dir, priv->build_uid, priv->build_gid);
	if (res != 0) {
		g_warning ("Could not adjust permissions on volatile /app dir: %s", g_strerror (errno));
		goto out;
	}

out:
	if (res != 0) {
		if (mount_target != NULL)
			umount (mount_target);
		if (app_mount_target != NULL)
			umount (app_mount_target);
	}

	return res;
}

/**
 * li_build_master_run_executor:
 *
 * Run the actual build setup and build steps as parent
 * process.
 */
static gint
li_build_master_run_executor (LiBuildMaster *bmaster, const gchar *env_root)
{
	gint res = 0;
	gboolean ret;
	gchar *tmp;
	g_autofree gchar *build_data_root = NULL;
	g_autofree gchar *newroot_dir = NULL;
	g_autofree gchar *volatile_data_dir = NULL;
	g_autofree gchar *ofs_wdir = NULL;
	g_autofree gchar *env_tmp_fname = NULL;
	gint env_fd = 0;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	tmp = g_strdup_printf ("Building %s - %s",
			       li_pkg_info_get_name (priv->pki),
			       li_pkg_info_get_version (priv->pki));
	li_build_master_print_section (bmaster, tmp);
	g_free (tmp);

	newroot_dir = li_run_env_setup_with_root (priv->chroot_orig_dir);
	if (!newroot_dir) {
		g_warning ("Unable to set up the environment.");
		goto out;
	}

	/* create our build directory */
	build_data_root = g_build_filename (newroot_dir, "build", NULL);
	res = g_mkdir_with_parents (build_data_root, 0755);
	if (res != 0) {
		g_warning ("Unable to set up the environment: %s", g_strerror (errno));
		goto out;
	}

	/* create volatile data dir (data which is generated during build) */
	volatile_data_dir = g_build_filename (env_root, "volatile", NULL);
	res = g_mkdir_with_parents (volatile_data_dir, 0755);
	if (res != 0) {
		g_warning ("Unable to set up the environment: %s", g_strerror (errno));
		goto out;
	}

	/* create OverlayFS work dir */
	ofs_wdir = g_build_filename (env_root, "ofs_work", NULL);
	res = g_mkdir_with_parents (ofs_wdir, 0755);
	if (res != 0) {
		g_warning ("Unable to set up the environment: %s", g_strerror (errno));
		goto out;
	}

	/* now mount our build-data directory via OverlayFS */
	tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s,workdir=%s", priv->build_root, volatile_data_dir, ofs_wdir);
	res = mount ("overlay", build_data_root,
			 "overlay", MS_MGC_VAL | MS_NOSUID, tmp);
	g_free (tmp);
	if (res != 0) {
		g_warning ("Unable to set up the environment: %s", g_strerror (errno));
		goto out;
	}

	/* overlay the base filesystem with the build-dependency data */
	res = li_build_master_mount_deps (bmaster, newroot_dir, env_root);
	if (res != 0) {
		g_warning ("Unable to set up the environment: %s", g_strerror (errno));
		goto out;
	}

	/* ensure we can write volatile data */
	res = chown (volatile_data_dir, priv->build_uid, priv->build_gid);
	if (res != 0) {
		g_warning ("Could not adjust permissions on volatile data dir: %s", g_strerror (errno));
		goto out;
	}

	if (!li_run_env_enter (newroot_dir)) {
		g_warning ("Could not enter build environment.");
		goto out;
	}

	/* set the correct build root in the chroot environment */
	g_free (build_data_root);
	build_data_root = g_strdup ("/build");

	ret = g_setenv ("BUILDROOT", build_data_root, TRUE);
	res = g_chdir (build_data_root);
	if ((!ret) || (res != 0)) {
		g_warning ("Unable to set up the environment!");
		goto out;
	}

	/* set linker paths and PATH */
	li_run_env_set_path_variables ();

	/* try to initialize groups, failure is not fatal */
	if (priv->build_uid > 0) {
		struct passwd *upws;

		upws = getpwuid (priv->build_uid);
		if (upws != NULL) {
			if (initgroups (upws->pw_name, priv->build_gid) < 0)
				g_warning ("Unable to initialize user groups: %s", g_strerror (errno));
		} else {
			g_warning ("Unable to initialize user groups: Could not determine user name: %s", g_strerror (errno));
		}
	}
	/* we now finished everything we needed root for, so drop root in case we build as user */
	if (setgid (priv->build_gid) != 0) {
		g_warning ("Unable to set gid: %s", g_strerror (errno));
		goto out;
	}
	if (setuid (priv->build_uid) != 0) {
		g_warning ("Unable to set uid: %s", g_strerror (errno));
		goto out;
	}

	/* ensure the details about the person we are building for are properly set */
	li_env_set_user_details (priv->username,
				 priv->email,
				 priv->target_repo);

	/* prepare our environment-file hack */
	env_tmp_fname = g_strdup ("/tmp/environment-XXXXXX");
	env_fd = mkstemp (env_tmp_fname);
	if (env_fd < 0) {
		g_error ("Unable to create environment file: %s", g_strerror (errno));
		res = 1;
		goto out;
	}
	res = g_chmod (env_tmp_fname, 0775);
	if (res < 0) {
		g_error ("Unable to set permissions: %s", g_strerror (errno));
		goto out;
	}

	/* now start running the actual commands */
	li_build_master_print_section (bmaster, "Preparing Build Environment");
	if (priv->cmds_pre != NULL) {
		res = li_build_master_exec_command_sequence (bmaster,
								"prepare",
								priv->cmds_pre,
								env_tmp_fname);
		if (res != 0)
			goto out;
	}

	if (priv->get_shell) {
		g_autofree gchar *cmd = NULL;

		g_debug ("Starting new shell session...");
		cmd = g_strdup_printf ("sh -sc '. %s'", env_tmp_fname);
		system (cmd);
	} else {
		/* we don't start an interactive shell, and get to business instead */
		li_build_master_print_section (bmaster, "Build");
		res = li_build_master_exec_command_sequence (bmaster,
								"build",
								priv->cmds,
								env_tmp_fname);
		if (res != 0)
			goto out;
	}

	li_build_master_print_section (bmaster, "Cleanup");
	if (priv->cmds_post != NULL) {
		res = li_build_master_exec_command_sequence (bmaster,
								"cleanup",
								priv->cmds_post,
								env_tmp_fname);
		if (res != 0)
			goto out;
	}

out:
	if (newroot_dir != NULL) {
		tmp = g_build_filename (newroot_dir, "build", NULL);
		umount (tmp);
		g_free (tmp);
	}
	if (env_fd > 0)
		close (env_fd);

	return res;
}

/**
 * li_build_master_run:
 */
gint
li_build_master_run (LiBuildMaster *bmaster, GError **error)
{
	gint res = 0;
	pid_t pid;
	gint child_status;
        pid_t ret_val;
	gchar *tmp;
	GError *tmp_error = NULL;
	GPtrArray *artifacts = NULL;
	g_autofree gchar *env_root = NULL;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	/* create the essential directories for the new build environment */
	g_debug ("Creating essential directories");
	tmp = li_get_uuid_string ();
	env_root = g_build_filename (LOCALSTATEDIR, "cache", "limba-build", "env", tmp, NULL);
	g_free (tmp);

	res = g_mkdir_with_parents (env_root, 0755);
	if (res != 0) {
		g_warning ("Unable to create build environment: %s", g_strerror (errno));
		goto out;
	}

	/* get details about who we are building this for */
	g_free (priv->email);
	g_free (priv->username);
	g_free (priv->target_repo);
	priv->email = li_env_get_user_email ();
	priv->username = li_env_get_user_fullname ();
	priv->target_repo = li_env_get_target_repo ();

	/* TODO: Get nice, reproducible name for a build job to set as scope name */
	g_debug ("Adding build job to new scope");
	li_add_to_new_scope ("limba-build", "1", &tmp_error);
	if (tmp_error != NULL) {
		g_warning ("Unable to add build job to scope: %s", tmp_error->message);
		res = 6;
		g_error_free (tmp_error);
		goto out;
	}

	g_debug ("Forking build executor");

	/* fork our build helper */
	pid = fork ();
	if (pid == 0) {
		/* child process */

		res = li_build_master_run_executor (bmaster, env_root);
		exit (res);
	} else if (pid < 0) {
		/* error */
		g_set_error_literal (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_INIT,
				"Unable to fork.");
	}

	/* wait for the build executor to terminate */
	while (TRUE) {
		ret_val = waitpid (pid, &child_status, 0);
		if (ret_val > 0) {
			if (WIFEXITED (ret_val))
				res = WEXITSTATUS (child_status);
			else
				res = child_status;
			break;
		} else if (ret_val < 0) {
			g_set_error (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_FAILED,
				"Waiting for build executor failed: %s", g_strerror (errno));
			break;
		}
	}

	if (priv->get_shell) {
		g_debug ("Shell session executor is done, finalizing...");
		goto finish;
	}

	g_debug ("Executor is done, rescuing build artifacts...");
	tmp = g_build_filename (env_root, "volatile", "lipkg", NULL);
	if (g_file_test (tmp, G_FILE_TEST_IS_DIR))
		artifacts = li_utils_find_files_matching (tmp, "*.ipk*", FALSE);
	g_free (tmp);
	if ((artifacts == NULL) || (artifacts->len == 0)) {
		g_print ("Unable to find build artifacts!\n");
	} else {
		guint i;
		for (i = 0; i < artifacts->len; i++) {
			gchar *fname;
			gchar *fname_dest;
			fname = (gchar*) g_ptr_array_index (artifacts, i);

			tmp = g_path_get_basename (fname);
			fname_dest = g_build_filename (priv->build_root, "lipkg", tmp, NULL);

			g_remove (fname_dest);
			li_copy_file (fname, fname_dest, &tmp_error);
			if (tmp_error != NULL) {
				g_warning ("Unable to copy build artifact from '%s': %s", fname, tmp_error->message);
				g_error_free (tmp_error);
				tmp_error = NULL;
			} else {
				g_print ("Stored: %s\n", tmp);
			}

			g_free (tmp);
			g_free (fname_dest);
		}
	}


finish:
	g_debug ("Unmounting...");
	tmp = g_build_filename (env_root, "chroot", NULL);
	umount (tmp);
	g_free (tmp);

	g_debug ("Removing build directory.");
	li_delete_dir_recursive (env_root);

out:
	if ((res != 0) && (*error == NULL))
		g_set_error_literal (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_STEP_FAILED,
				_("Build command failed with non-zero exit status."));

	if (res > 255)
		res = 2;

	return res;
}

/**
 * li_build_master_get_shell:
 *
 * Get an interactive shell in the build environment, instead of building the software.
 */
gint
li_build_master_get_shell (LiBuildMaster *bmaster, GError **error)
{
	GError *tmp_error = NULL;
	gint res;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	priv->get_shell = TRUE;
	res = li_build_master_run (bmaster, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

out:
	priv->get_shell = FALSE;
	return res;
}

/**
 * li_build_master_set_build_user:
 */
void
li_build_master_set_build_user (LiBuildMaster *bmaster, uid_t uid)
{
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);
	priv->build_uid = uid;
}

/**
 * li_build_master_set_build_group:
 */
void
li_build_master_set_build_group (LiBuildMaster *bmaster, gid_t gid)
{
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);
	priv->build_gid = gid;
}

/**
 * li_build_master_set_ignore_foundations:
 */
void
li_build_master_set_ignore_foundations (LiBuildMaster *bmaster, gboolean ignore)
{
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);
	priv->ignore_foundations = ignore;
}

/**
 * li_build_master_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_build_master_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiBuildMasterError");
	return quark;
}

/**
 * li_build_master_class_init:
 **/
static void
li_build_master_class_init (LiBuildMasterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_build_master_finalize;
}

/**
 * li_build_master_new:
 *
 * Creates a new #LiBuildMaster.
 *
 * Returns: (transfer full): a #LiBuildMaster
 *
 **/
LiBuildMaster *
li_build_master_new (void)
{
	LiBuildMaster *bmaster;
	bmaster = g_object_new (LI_TYPE_BUILD_MASTER, NULL);
	return LI_BUILD_MASTER (bmaster);
}
