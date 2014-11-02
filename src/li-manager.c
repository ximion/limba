/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:li-manager
 * @short_description: Work with mgralled software
 */

#include "config.h"
#include "li-manager.h"

#include "li-utils-private.h"
#include "li-pkg-info.h"
#include "li-config.h"

typedef struct _LiManagerPrivate	LiManagerPrivate;
struct _LiManagerPrivate
{
	GPtrArray *installed_sw;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiManager, li_manager, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_manager_get_instance_private (o))

/**
 * li_manager_finalize:
 **/
static void
li_manager_finalize (GObject *object)
{
	LiManager *mgr = LI_MANAGER (object);
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	g_ptr_array_unref (priv->installed_sw);

	G_OBJECT_CLASS (li_manager_parent_class)->finalize (object);
}

/**
 * li_manager_init:
 **/
static void
li_manager_init (LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	priv->installed_sw = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * li_manager_find_installed_software:
 **/
static gboolean
li_manager_find_installed_software (LiManager *mgr)
{
	GError *tmp_error = NULL;
	GFile *fdir;
	GFileEnumerator *enumerator = NULL;
	GFileInfo *file_info;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	/* get stuff in the software directory */
	fdir =  g_file_new_for_path (LI_INSTALL_ROOT);
	enumerator = g_file_enumerate_children (fdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &tmp_error);
	if (tmp_error != NULL)
		goto out;

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &tmp_error)) != NULL) {
		gchar *path;
		if (tmp_error != NULL)
			goto out;

		if (g_file_info_get_is_hidden (file_info))
			continue;
		path = g_build_filename (LI_INSTALL_ROOT,
								 g_file_info_get_name (file_info),
								 "control",
								 NULL);
		if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			GFile *ctlfile;
			ctlfile = g_file_new_for_path (path);
			if (g_file_query_exists (ctlfile, NULL)) {
				LiPkgInfo *ctl;
				ctl = li_pkg_info_new ();
				li_pkg_info_load_file (ctl, ctlfile);
				g_ptr_array_add (priv->installed_sw, ctl);
			}
			g_object_unref (ctlfile);
		}
		g_free (path);
	}


out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
	if (tmp_error != NULL) {
		g_printerr ("Error while searching for installed software: %s\n", tmp_error->message);
		return FALSE;
	}

	return TRUE;
}

/**
 * li_manager_get_installed_software:
 **/
GPtrArray*
li_manager_get_installed_software (LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	if (priv->installed_sw->len == 0) {
		/* in case no software was found or we never searched for it, we
		 * do this again
		 */
		li_manager_find_installed_software (mgr);
	}

	return priv->installed_sw;
}

/**
 * li_manager_class_init:
 **/
static void
li_manager_class_init (LiManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_manager_finalize;
}

/**
 * li_manager_new:
 *
 * Creates a new #LiManager.
 *
 * Returns: (transfer full): a #LiManager
 *
 **/
LiManager *
li_manager_new (void)
{
	LiManager *mgr;
	mgr = g_object_new (LI_TYPE_MANAGER, NULL);
	return LI_MANAGER (mgr);
}
