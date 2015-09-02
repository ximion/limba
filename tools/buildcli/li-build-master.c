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

#include "li-build-master.h"

#include <config.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <stdlib.h>
#include "li-build-conf.h"


typedef struct _LiBuildMasterPrivate	LiBuildMasterPrivate;
struct _LiBuildMasterPrivate
{
	gchar *build_root;
	gboolean init_done;

	GPtrArray *cmds_pre;
	GPtrArray *cmds;
	GPtrArray *cmds_post;
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
	if (priv->cmds_pre != NULL)
		g_ptr_array_unref (priv->cmds_pre);
	if (priv->cmds != NULL)
		g_ptr_array_unref (priv->cmds);
	if (priv->cmds_post != NULL)
		g_ptr_array_unref (priv->cmds_post);

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
li_build_master_init_build (LiBuildMaster *bmaster, const gchar *dir, GError **error)
{
	LiBuildConf *bconf;
	GError *tmp_error = NULL;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

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
 * li_build_master_run:
 */
gint
li_build_master_run (LiBuildMaster *bmaster, GError **error)
{
	guint i;
	gint res = 0;
	gboolean ret;
	LiBuildMasterPrivate *priv = GET_PRIVATE (bmaster);

	ret = g_setenv ("BUILDROOT", priv->build_root, TRUE);
	res = g_chdir (priv->build_root);
	if ((!ret) || (res != 0)) {
		g_warning ("Unable to set up the environment!");
		goto out;
	}

	li_build_master_print_section (bmaster, "Preparing Build Environment");
	for (i = 0; i < priv->cmds_pre->len; i++) {
		gchar *cmd;
		cmd = (gchar*) g_ptr_array_index (priv->cmds_pre, i);
		res = li_build_master_exec (bmaster, cmd);
		if (res != 0)
			goto out;
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
	for (i = 0; i < priv->cmds_post->len; i++) {
		gchar *cmd;
		cmd = (gchar*) g_ptr_array_index (priv->cmds_post, i);
		res = li_build_master_exec (bmaster, cmd);
		if (res != 0)
			goto out;
	}

out:
	if (res != 0)
		g_set_error_literal (error,
					LI_BUILD_MASTER_ERROR,
					LI_BUILD_MASTER_ERROR_STEP_FAILED,
					_("Build command failed with non-zero exit status."));

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
