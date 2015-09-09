/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Matthias Klumpp <matthias@tenstral.net>
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

#include "li-utils-private.h"
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

	gchar *username;
	gchar *email;
	gchar *target_repo;
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
}

/**
 * li_build_master_init_build:
 */
void
li_build_master_init_build (LiBuildMaster *bmaster, const gchar *dir, const gchar *chroot_orig, GError **error)
{
	LiBuildConf *bconf;
	GError *tmp_error = NULL;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	if (chroot_orig != NULL)
		priv->chroot_orig_dir = g_strdup (chroot_orig);

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
		g_object_unref (bconf);
		return;
	}

	priv->cmds = li_build_conf_get_script (bconf);
	if (priv->cmds == NULL) {
		g_set_error_literal (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_NO_COMMANDS,
				_("Could not find commands to build this application!"));
		g_object_unref (bconf);
		return;
	}

	priv->cmds_pre = li_build_conf_get_before_script (bconf);
	priv->cmds_post = li_build_conf_get_after_script (bconf);

	priv->build_root = g_strdup (dir);

	priv->init_done = TRUE;
	g_object_unref (bconf);
}

/**
 * li_copy_directory_recusrive:
 */
static gint
li_copy_directory_recusrive (const gchar *srcdir, const gchar *destdir)
{
	gchar *cmd;
	gint res;

	/* FIXME: Write new code which does not involve calling cp via system() */

	cmd = g_strdup_printf ("cp -dpr %s/ %s", srcdir, destdir);
	res = system (cmd);
	g_free (cmd);

	return res;
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
}

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
	guint i;
	gchar *tmp;
	guint mount_count = 0;
	_cleanup_free_ gchar *build_tmp_root = NULL;
	_cleanup_free_ gchar *chroot_dir = NULL;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	build_tmp_root = g_build_filename (env_root, "build", NULL);

	/* copy over the sources to not taint the original source tree */
	res = li_copy_directory_recusrive (priv->build_root, build_tmp_root);
	if (res != 0) {
		g_warning ("Unable to set up the environment: %s", g_strerror (errno));
		goto out;
	}

	if (priv->chroot_orig_dir != NULL) {
		_cleanup_free_ gchar *ofs_wdir = NULL;
		_cleanup_free_ gchar *volatile_data_dir = NULL;

		/* yay, we are building in a chroot environment! */

		volatile_data_dir = g_build_filename (env_root, "volatile", NULL);
		ofs_wdir = g_build_filename (env_root, "ofs_work", NULL);
		chroot_dir = g_build_filename (env_root, "chroot", NULL);

		/* create our build directory and volatile files directory */
		tmp = g_build_filename (volatile_data_dir, "build", NULL);
		res = g_mkdir_with_parents (tmp, 0755);
		g_free (tmp);
		if (res != 0) {
			g_warning ("Unable to set up the environment: %s", g_strerror (errno));
			goto out;
		}

		/* create OverlayFS work dir */
		res = g_mkdir_with_parents (ofs_wdir, 0755);
		if (res != 0) {
			g_warning ("Unable to set up the environment: %s", g_strerror (errno));
			goto out;
		}

		/* create chroot dir */
		res = g_mkdir_with_parents (chroot_dir, 0755);
		if (res != 0) {
			g_warning ("Unable to set up the environment: %s", g_strerror (errno));
			goto out;
		}

		/* create new mount namespace */
		res = unshare (CLONE_NEWNS);
		if (res != 0) {
			g_warning ("Failed to create new namespace: %s", strerror(errno));
			goto out;
		}

		/* create a private mountpoint */
		res = mount (chroot_dir, chroot_dir,
				 NULL, MS_PRIVATE, NULL);
		if (res != 0 && errno == EINVAL) {
			/* maybe we can't make the mountpoint private yet? */
			res = mount (chroot_dir, chroot_dir,
					 NULL, MS_BIND, NULL);
			/* try again */
			if (res == 0) {
				mount_count++;
				res = mount (chroot_dir, chroot_dir,
						 NULL, MS_PRIVATE, NULL);
			}
		}
		mount_count++;
		if (res != 0) {
			g_warning ("Unable to create private mountpoint: %s", g_strerror (errno));
			goto out;
		}
		g_debug ("%s", chroot_dir);

		/* mount our chroot environment */
		tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s,workdir=%s", priv->chroot_orig_dir, volatile_data_dir, ofs_wdir);
		res = mount ("overlay", chroot_dir,
				 "overlay", MS_MGC_VAL | MS_NOSUID, tmp);
		g_free (tmp);
		if (res != 0) {
			g_warning ("Unable to set up the environment: %s", g_strerror (errno));
			goto out;
		}

		/* now bind-mount our build data into the chroot */
		tmp = g_build_filename (chroot_dir, "build", NULL);
		res = mount (build_tmp_root, tmp,
					 NULL, MS_BIND, NULL);
		g_free (tmp);
		if (res != 0) {
			g_warning ("Unable to set up the environment: %s", g_strerror (errno));
			goto out;
		}

		res = chroot (chroot_dir);
		g_chdir ("/");
		if (res != 0) {
			g_warning ("Could not chroot: %s", g_strerror (errno));
			goto out;
		}

		/* set the correct build root in the chroot environment */
		g_free (build_tmp_root);
		build_tmp_root = g_strdup ("/build");
	}

	ret = g_setenv ("BUILDROOT", build_tmp_root, TRUE);
	res = g_chdir (build_tmp_root);
	if ((!ret) || (res != 0)) {
		g_warning ("Unable to set up the environment!");
		goto out;
	}

	/* ensure the details about the person we are building for are properly set */
	li_env_set_user_details (priv->username,
				 priv->email,
				 priv->target_repo);

	li_build_master_print_section (bmaster, "Preparing Build Environment");
	if (priv->cmds_pre != NULL) {
		for (i = 0; i < priv->cmds_pre->len; i++) {
			gchar *cmd;
			cmd = (gchar*) g_ptr_array_index (priv->cmds_pre, i);
			res = li_build_master_exec (bmaster, cmd);
			if (res != 0)
				goto out;
		}
	}

	li_build_master_print_section (bmaster, "Build");
	for (i = 0; i < priv->cmds->len; i++) {
		gchar *cmd;
		cmd = (gchar*) g_ptr_array_index (priv->cmds, i);
		res = li_build_master_exec (bmaster, cmd);
		if (res != 0)
			goto out;
	}

	li_build_master_print_section (bmaster, "Cleanup");
	if (priv->cmds_post != NULL) {
		for (i = 0; i < priv->cmds_post->len; i++) {
			gchar *cmd;
			cmd = (gchar*) g_ptr_array_index (priv->cmds_post, i);
			res = li_build_master_exec (bmaster, cmd);
			if (res != 0)
				goto out;
		}
	}

out:
	if (chroot_dir != NULL) {
		tmp = g_build_filename (chroot_dir, "build", NULL);
		umount (tmp);
		g_free (tmp);
		while (mount_count-- > 0)
			umount (chroot_dir);
	}

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
	_cleanup_free_ gchar *env_root = NULL;
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

	/* for our build helper */
	pid = fork ();

	if (pid == 0) {
		/* child process */
		res = li_build_master_run_executor (bmaster, env_root);
		exit (res);
	} else if (pid < 0) {
		/* error */
		g_set_error_literal (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_FAILED,
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

	g_debug ("Executor is done, rescuing build artifacts...");
	tmp = g_build_filename (env_root, "build", "lipkg", NULL);
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


	g_debug ("Unmounting...");
	tmp = g_build_filename (env_root, "chroot", NULL);
	umount (tmp);
	g_free (tmp);

	g_debug ("Removing build directory.");
	li_delete_dir_recursive (env_root);

out:
	if (res != 0)
		g_set_error_literal (error,
				LI_BUILD_MASTER_ERROR,
				LI_BUILD_MASTER_ERROR_STEP_FAILED,
				_("Build command failed with non-zero exit status."));

	if (res > 255)
		res = 2;

	return res;
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
