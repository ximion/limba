/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
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

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "li-utils.h"
#include "li-utils-private.h"
#include "li-pkg-info.h"
#include "li-pkg-cache.h"
#include "li-keyring.h"
#include "li-installer.h"
#include "li-update-item.h"

#include "li-dbus-interface.h"

#define LI_CLEANUP_HINT_FNAME "/var/lib/limba/cleanup-needed"

typedef struct _LiManagerPrivate	LiManagerPrivate;
struct _LiManagerPrivate
{
	GHashTable *pkgs; /* key:utf8;value:LiPkgInfo */
	GPtrArray *rts; /* of LiRuntime */
	GHashTable *updates; /* of LiUpdateItem */

	/* DBus helper */
	GMainLoop *loop;
	LiProxyManager *bus_proxy;
	GError *proxy_error;
	guint bus_watch_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiManager, li_manager, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (li_manager_get_instance_private (o))

enum {
	SIGNAL_PROGRESS,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

/**
 * li_manager_finalize:
 **/
static void
li_manager_finalize (GObject *object)
{
	LiManager *mgr = LI_MANAGER (object);
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	g_hash_table_unref (priv->pkgs);
	g_ptr_array_unref (priv->rts);
	g_hash_table_unref (priv->updates);

	g_main_loop_unref (priv->loop);
	if (priv->proxy_error != NULL)
		g_error_free (priv->proxy_error);
	if (priv->bus_proxy != NULL)
		g_object_unref (priv->bus_proxy);

	G_OBJECT_CLASS (li_manager_parent_class)->finalize (object);
}

/**
 * li_pki_hash_func:
 *
 * GHashFunc() for #LiPkgInfo
 */
static guint
li_pki_hash_func (LiPkgInfo *key)
{
	return g_str_hash (li_pkg_info_get_id (key));
}

/**
 * li_pkg_equal_func:
 *
 * GEqualFunc() for #LiPkgInfo
 */
static gboolean
li_pki_equal_func (LiPkgInfo *a, LiPkgInfo *b)
{
	return g_strcmp0 (li_pkg_info_get_id (a), li_pkg_info_get_id (b)) == 0;
}

/**
 * li_manager_init:
 **/
static void
li_manager_init (LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	priv->pkgs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->rts = g_ptr_array_new_with_free_func (g_object_unref);
	priv->updates = g_hash_table_new_full ((GHashFunc) li_pki_hash_func,
						(GEqualFunc) li_pki_equal_func,
						g_object_unref,
						g_object_unref);
	priv->loop = g_main_loop_new (NULL, FALSE);
}

/**
 * li_manager_reset_cached_data:
 */
static void
li_manager_reset_cached_data (LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	g_hash_table_unref (priv->pkgs);
	g_ptr_array_unref (priv->rts);
	priv->pkgs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	priv->rts = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * li_manager_find_installed_software:
 **/
static GHashTable*
li_manager_get_installed_software (LiManager *mgr, GError **error)
{
	GError *tmp_error = NULL;
	g_autoptr(GFile) fdir = NULL;
	g_autoptr(GFileEnumerator) enumerator = NULL;
	GFileInfo *file_info;
	GHashTable *pkgs = NULL;

	if (!g_file_test (LI_SOFTWARE_ROOT, G_FILE_TEST_IS_DIR)) {
		/* directory not found, no software to be searched for */
		return NULL;
	}

	/* get stuff in the software directory */
	fdir = g_file_new_for_path (LI_SOFTWARE_ROOT);
	enumerator = g_file_enumerate_children (fdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &tmp_error);
	if (tmp_error != NULL)
		goto out;

	pkgs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &tmp_error)) != NULL) {
		g_autoptr(GFile) fsdir = NULL;
		g_autoptr(GFileEnumerator) senum = NULL;
		g_autofree gchar *mpath = NULL;
		GFileInfo *s_finfo;

		if (tmp_error != NULL)
			goto out;
		if (g_file_info_get_is_hidden (file_info))
			continue;
		/* ignore the runtimes directory */
		if (g_strcmp0 (g_file_info_get_name (file_info), "runtimes") == 0)
			continue;

		mpath = g_build_filename (LI_SOFTWARE_ROOT, g_file_info_get_name (file_info), NULL);

		fsdir = g_file_new_for_path (mpath);
		senum = g_file_enumerate_children (fsdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &tmp_error);
		if (tmp_error != NULL)
			goto out;

		while ((s_finfo = g_file_enumerator_next_file (senum, NULL, &tmp_error)) != NULL) {
			g_autofree gchar *cpath = NULL;

			if (tmp_error != NULL)
				goto out;
			if (g_file_info_get_is_hidden (s_finfo))
				continue;

			cpath = g_build_filename (mpath,
						g_file_info_get_name (s_finfo),
						"control",
						NULL);

			if (g_file_test (cpath, G_FILE_TEST_IS_REGULAR)) {
				g_autoptr(GFile) ctlfile = NULL;
				ctlfile = g_file_new_for_path (cpath);
				if (g_file_query_exists (ctlfile, NULL)) {
					LiPkgInfo *pki;

					/* create new LiPkgInfo for an installed package */
					pki = li_pkg_info_new ();

					li_pkg_info_load_file (pki, ctlfile, &tmp_error);
					if (tmp_error != NULL) {
						g_object_unref (pki);
						goto out;
					}

					/* do not list as installed if the software is faded */
					if (!li_pkg_info_has_flag (pki, LI_PACKAGE_FLAG_FADED)) {
						li_pkg_info_add_flag (pki, LI_PACKAGE_FLAG_INSTALLED);
					}

					g_hash_table_insert (pkgs,
								g_strdup (li_pkg_info_get_id (pki)),
								pki);
				}
			}
		}
	}

out:
	if (tmp_error != NULL) {
		g_propagate_prefixed_error (error,
					tmp_error,
					"Error while searching for installed software:");
		if (pkgs != NULL)
			g_hash_table_unref (pkgs);
		return NULL;
	}

	return pkgs;
}

/**
 * li_manager_update_packages_table:
 */
static void
li_manager_update_software_table (LiManager *mgr, GError **error)
{
	g_autoptr(LiPkgCache) cache = NULL;
	GPtrArray *apkgs;
	GHashTable *ipkgs;
	guint i;
	GError *tmp_error = NULL;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	if (g_hash_table_size (priv->pkgs) > 0) {
		/* we have cached data, so no need to search for it again */
		return;
	}

	cache = li_pkg_cache_new ();

	li_pkg_cache_open (cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	/* populate table with installed packages */
	ipkgs = li_manager_get_installed_software (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	if (ipkgs != NULL) {
		g_hash_table_unref (priv->pkgs);
		priv->pkgs = ipkgs;
	}

	/* get available packages */
	apkgs = li_pkg_cache_get_packages (cache);
	for (i = 0; i < apkgs->len; i++) {
		LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (apkgs, i));

		/* add packages to the global list, if they are not already installed */
		if (g_hash_table_lookup (priv->pkgs, li_pkg_info_get_id (pki)) == NULL) {
			g_hash_table_insert (priv->pkgs,
						g_strdup (li_pkg_info_get_id (pki)),
						g_object_ref (pki));
		}
	}
}

/**
 * li_manager_get_software_list:
 *
 * Returns: (transfer container) (element-type LiPkgInfo): A list of installed and available software
 **/
GList*
li_manager_get_software_list (LiManager *mgr, GError **error)
{
	GError *tmp_error = NULL;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	li_manager_update_software_table (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	return g_hash_table_get_values (priv->pkgs);
}

/**
 * li_manager_get_software_by_pkid:
 *
 * Returns: (transfer none): A #LiPkgInfo or %NULL if no software was found.
 **/
LiPkgInfo*
li_manager_get_software_by_pkid (LiManager *mgr, const gchar *pkid, GError **error)
{
	GError *tmp_error = NULL;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	li_manager_update_software_table (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	return g_hash_table_lookup (priv->pkgs, pkid);
}

/**
 * li_manager_find_installed_runtimes:
 **/
static gboolean
li_manager_find_installed_runtimes (LiManager *mgr)
{
	GError *tmp_error = NULL;
	GFile *fdir;
	GFileEnumerator *enumerator = NULL;
	GFileInfo *file_info;
	g_autofree gchar *runtime_root;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	runtime_root = g_build_filename (LI_SOFTWARE_ROOT, "runtimes", NULL);
	if (!g_file_test (runtime_root, G_FILE_TEST_IS_DIR)) {
		/* directory not found, no software to be searched for */
		return TRUE;
	}

	/* get stuff in the software-runtime directory */
	fdir =  g_file_new_for_path (runtime_root);
	enumerator = g_file_enumerate_children (fdir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &tmp_error);
	if (tmp_error != NULL)
		goto out;

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &tmp_error)) != NULL) {
		g_autofree gchar *rt_path = NULL;
		if (tmp_error != NULL)
			goto out;

		if (g_file_info_get_is_hidden (file_info))
			continue;
		rt_path = g_build_filename (runtime_root, g_file_info_get_name (file_info), NULL);
		if (g_file_test (rt_path, G_FILE_TEST_IS_REGULAR)) {
			gboolean ret;
			LiRuntime *rt;

			rt = li_runtime_new ();
			ret = li_runtime_load_from_file (rt, rt_path, &tmp_error);
			if (ret)
				g_ptr_array_add (priv->rts, g_object_ref (rt));

			g_object_unref (rt);
		}
	}


out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
	if (tmp_error != NULL) {
		g_printerr ("Error while searching for installed runtimes: %s\n", tmp_error->message);
		g_error_free (tmp_error);
		return FALSE;
	}

	return TRUE;
}

/**
 * li_manager_get_installed_runtimes:
 *
 * Returns: (transfer none) (element-type LiRuntime): A list of registered runtimes
 **/
GPtrArray*
li_manager_get_installed_runtimes (LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	if (priv->rts->len == 0) {
		/* in case no runtime was found or we never searched for it, we
		 * do this again
		 */
		li_manager_find_installed_runtimes (mgr);
	}

	return priv->rts;
}

/**
 * li_manager_find_runtime_with_members:
 * @mgr: An instance of #LiManager
 * @members: (element-type LiPkgInfo): Software components which should be present in the runtime
 *
 * Get an installed runtime which contains the specified members.
 * If none is available, %NULL is returned.
 * The resulting runtime needs to be unref'ed with g_object_unref()
 * if it is no longer needed.
 *
 * Returns: (transfer full): A #LiRuntime containing @members or %NULL
 */
LiRuntime*
li_manager_find_runtime_with_members (LiManager *mgr, GPtrArray *members)
{
	guint i, j;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	/* ensure we have all installed runtimes cached */
	li_manager_get_installed_runtimes (mgr);

	/* NOTE: If we ever have more frameworks with more members, we might need a more efficient implementation here */
	for (i = 0; i < priv->rts->len; i++) {
		GHashTable *test_members;
		gboolean ret = FALSE;
		LiRuntime *rt = LI_RUNTIME (g_ptr_array_index (priv->rts, i));

		test_members = li_runtime_get_members (rt);
		for (j = 0; j < members->len; j++) {
			const gchar *pkid;
			LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (members, j));
			pkid = li_pkg_info_get_id (pki);

			ret = g_hash_table_lookup (test_members, pkid) != NULL;
			if (!ret)
				break;
		}

		if (ret)
			return g_object_ref (rt);
	}

	return NULL;
}

/**
 * li_manager_find_runtimes_with_member:
 * @mgr: An instance of #LiManager
 */
static GPtrArray*
li_manager_find_runtimes_with_member (LiManager *mgr, LiPkgInfo *member)
{
	guint i;
	GPtrArray *res = NULL;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	/* ensure we have all installed runtimes cached */
	li_manager_get_installed_runtimes (mgr);

	/* NOTE: If we ever have more frameworks with more members, we might need a more efficient implementation here */
	for (i = 0; i < priv->rts->len; i++) {
		GHashTable *test_members;
		gboolean ret = FALSE;
		LiRuntime *rt = LI_RUNTIME (g_ptr_array_index (priv->rts, i));

		test_members = li_runtime_get_members (rt);
		ret = g_hash_table_lookup (test_members, li_pkg_info_get_id (member)) != NULL;
		if (ret) {
			if (res == NULL)
				res = g_ptr_array_new_with_free_func (g_object_unref);
			g_ptr_array_add (res, g_object_ref (rt));
		}
	}

	return res;
}

/**
 * li_manager_remove_exported_files:
 */
static void
li_manager_remove_exported_files (GFile *file, GError **error)
{
	gchar *line = NULL;
	GFileInputStream* ir;
	GDataInputStream* dis;
	gint res;

	ir = g_file_read (file, NULL, NULL);
	dis = g_data_input_stream_new ((GInputStream*) ir);
	g_object_unref (ir);

	while (TRUE) {
		g_auto(GStrv) parts = NULL;
		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		parts = g_strsplit (line, "\t", 2);
		if (parts[1] == NULL)
			continue;

		if (g_str_has_prefix (parts[1], "/")) {
			/* don't fail if file is already gone */
			if (!g_file_test (parts[1], G_FILE_TEST_EXISTS))
				continue;
			/* delete file */
			res = g_remove (parts[1]);
			if (res != 0) {
				g_set_error (error,
					LI_MANAGER_ERROR,
					LI_MANAGER_ERROR_REMOVE_FAILED,
					_("Could not delete file '%s'"), parts[1]);
				goto out;
			}
		}
	}

out:
	g_object_unref (dis);
}

/**
 * li_manager_proxy_progress_cb:
 */
static void
li_manager_proxy_progress_cb (LiProxyManager *mgr_bus, const gchar *id, gint percentage, LiManager *mgr)
{
	if (g_strcmp0 (id, "") == 0)
		id = NULL;

	g_signal_emit (mgr, signals[SIGNAL_PROGRESS], 0,
					percentage, id);
}

/**
 * li_manager_proxy_error_cb:
 *
 * Callback for the Error() DBus signal
 */
static void
li_manager_proxy_error_cb (LiProxyManager *mgr_bus, guint32 domain, guint code, const gchar *message, LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	/* ensure no error is set */
	if (priv->proxy_error != NULL) {
		g_error_free (priv->proxy_error);
		priv->proxy_error = NULL;
	}

	g_set_error (&priv->proxy_error, domain, code, "%s", message);
}

/**
 * li_manager_proxy_finished_cb:
 *
 * Callback for the Finished() DBus signal
 */
static void
li_manager_proxy_finished_cb (LiProxyManager *mgr_bus, gboolean success, LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	if (success) {
		/* ensure no error is set */
		if (priv->proxy_error != NULL) {
			g_error_free (priv->proxy_error);
			priv->proxy_error = NULL;
		}
	}

	g_main_loop_quit (priv->loop);
}

/**
 * li_manager_bus_vanished:
 */
static void
li_manager_bus_vanished (GDBusConnection *connection, const gchar *name, LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	/* check if we are actually running any action */
	if (!g_main_loop_is_running (priv->loop))
		return;

	if (priv->proxy_error == NULL) {
		/* set error, since the DBus name vanished while performing an action, indicating that something crashed */
		g_set_error (&priv->proxy_error,
			LI_INSTALLER_ERROR,
			LI_INSTALLER_ERROR_INTERNAL,
			_("The Limba daemon vanished from the bus mid-transaction, so it likely crashed. Please file a bug against Limba."));
	}

	g_main_loop_quit (priv->loop);
}

/**
 * li_manager_remove_software:
 **/
gboolean
li_manager_remove_software (LiManager *mgr, const gchar *pkgid, GError **error)
{
	g_autofree gchar *swpath = NULL;
	GFile *expfile;
	gchar *tmp;
	GError *tmp_error = NULL;
	g_autoptr(GPtrArray) pkg_array = NULL;
	GFile *ctlfile;
	LiPkgInfo *pki;
	LiRuntime *rt;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	if (!li_utils_is_root ()) {
		/* we do not have root privileges - call the helper daemon to install the package */
		g_debug ("Calling Limba DBus service.");

		if (priv->bus_proxy == NULL) {
			/* looks like we do not yet have a bus connection, so we create one */
			priv->bus_proxy = li_proxy_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
										G_DBUS_PROXY_FLAGS_NONE,
										"org.freedesktop.Limba",
										"/org/freedesktop/Limba/Manager",
										NULL,
										&tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}

			/* we want to know when the bus name vanishes unexpectedly */
			if (priv->bus_watch_id == 0) {
				priv->bus_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
									"org.freedesktop.Limba",
									G_BUS_NAME_WATCHER_FLAGS_NONE,
									NULL,
									(GBusNameVanishedCallback) li_manager_bus_vanished,
									mgr,
									NULL);
			}

			g_signal_connect (priv->bus_proxy, "progress",
						G_CALLBACK (li_manager_proxy_progress_cb), mgr);
			g_signal_connect (priv->bus_proxy, "error",
						G_CALLBACK (li_manager_proxy_error_cb), mgr);
			g_signal_connect (priv->bus_proxy, "finished",
						G_CALLBACK (li_manager_proxy_finished_cb), mgr);
		}

		/* ensure no error is set */
		if (priv->proxy_error != NULL) {
			g_error_free (priv->proxy_error);
			priv->proxy_error = NULL;
		}

		li_proxy_manager_call_remove_software_sync (priv->bus_proxy,
						pkgid,
						NULL,
						&tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			goto out;
		}

		g_main_loop_run (priv->loop);

		if (priv->proxy_error != NULL) {
			g_propagate_error (error, priv->proxy_error);
			priv->proxy_error = NULL;
			goto out;
		}

		goto out;
	}

	swpath = g_build_filename (LI_SOFTWARE_ROOT, pkgid, NULL);

	tmp = g_build_filename (swpath, "control", NULL);
	ctlfile = g_file_new_for_path (tmp);
	g_free (tmp);
	if (!g_file_query_exists (ctlfile, NULL)) {
		g_object_unref (ctlfile);
		g_set_error (error,
				LI_MANAGER_ERROR,
				LI_MANAGER_ERROR_NOT_FOUND,
				_("Could not find software: %s"), pkgid);
		return FALSE;
	}
	pki = li_pkg_info_new ();
	li_pkg_info_load_file (pki, ctlfile, &tmp_error);
	g_object_unref (ctlfile);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_object_unref (pki);
		return FALSE;
	}

	/* test if a runtime uses this software */
	pkg_array = g_ptr_array_new_with_free_func (g_object_unref);
	g_ptr_array_add (pkg_array, pki);
	rt = li_manager_find_runtime_with_members (mgr, pkg_array);
	if (rt != NULL) {
		g_autoptr(GList) sw = NULL;
		GList *l;
		gboolean dependency_found = FALSE;

		/* check if this software is in use somewhere */
		sw = li_manager_get_software_list (mgr, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}

		for (l = sw; l != NULL; l = l->next) {
			LiPkgInfo *pki2 = LI_PKG_INFO (l->data);

			/* check if the package is actually installed */
			if (!li_pkg_info_has_flag (pki2, LI_PACKAGE_FLAG_INSTALLED))
				continue;

			if (g_strcmp0 (li_pkg_info_get_runtime_dependency (pki2), li_runtime_get_uuid (rt)) == 0) {
				/* TODO: Emit broken packages here, don't misuse GError */
				g_set_error (error,
					LI_MANAGER_ERROR,
					LI_MANAGER_ERROR_DEPENDENCY,
						_("Removing '%s' would break at least '%s'."), pkgid, li_pkg_info_get_name (pki2));
				dependency_found = TRUE;
				break;
			}
		}

		if (dependency_found) {
			g_object_unref (rt);
			return FALSE;
		} else {
			/* apparently nothing uses this runtime anymore - remove it */
			li_runtime_remove (rt);
			g_debug ("Removed runtime: %s", li_runtime_get_uuid (rt));
		}
		g_object_unref (rt);
	}

	/* remove exported files */
	tmp = g_build_filename (swpath, "exported", NULL);
	expfile = g_file_new_for_path (tmp);
	g_free (tmp);
	if (g_file_query_exists (expfile, NULL)) {
		li_manager_remove_exported_files (expfile, &tmp_error);
	}
	g_object_unref (expfile);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* now delete the directory */
	if (!li_delete_dir_recursive (swpath)) {
		g_set_error (error,
				LI_MANAGER_ERROR,
				LI_MANAGER_ERROR_REMOVE_FAILED,
				_("Could not remove software directory."));
		return FALSE;
	}

	g_debug ("Removed package: %s", pkgid);

	/* we need to recreate the caches, now that the installed software has changed */
	li_manager_reset_cached_data (mgr);

out:
	return TRUE;
}

/**
 * li_manager_package_is_installed:
 *
 * Test if package is installed.
 */
gboolean
li_manager_package_is_installed (LiManager *mgr, LiPkgInfo *pki)
{
	g_autofree gchar *pkid = NULL;
	g_autofree gchar *path = NULL;

	pkid = g_strdup_printf ("%s/%s",
				li_pkg_info_get_name (pki), li_pkg_info_get_version (pki));
	path = g_build_filename (LI_SOFTWARE_ROOT, pkid, "control", NULL);

	if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
		return TRUE;

	return FALSE;
}

/**
 * li_manager_cleanup_broken_packages:
 *
 * Remove all invalid directories in /opt/software
 **/
static void
li_manager_cleanup_broken_packages (LiManager *mgr, GError **error)
{
	GError *tmp_error = NULL;
	g_autoptr(GFile) fdir = NULL;
	g_autoptr(GFileEnumerator) enumerator = NULL;
	GFileInfo *file_info;

	if (!g_file_test (LI_SOFTWARE_ROOT, G_FILE_TEST_IS_DIR)) {
		/* directory not found, no broken software to be found */
		return;
	}

	/* get stuff in the software directory */
	fdir = g_file_new_for_path (LI_SOFTWARE_ROOT);
	enumerator = g_file_enumerate_children (fdir,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						NULL,
						&tmp_error);
	if (tmp_error != NULL)
		goto out;

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &tmp_error)) != NULL) {
		g_autoptr(GFile) fsdir = NULL;
		g_autoptr(GFileEnumerator) senum = NULL;
		g_autofree gchar *mpath = NULL;
		gint child_count = 0;
		GFileInfo *s_finfo;

		if (tmp_error != NULL)
			goto out;
		if (g_file_info_get_is_hidden (file_info))
			continue;
		/* the "runtimes" directory is special, never remove it */
		if (g_strcmp0 (g_file_info_get_name (file_info), "runtimes") == 0)
			continue;

		mpath = g_build_filename (LI_SOFTWARE_ROOT, g_file_info_get_name (file_info), NULL);

		fsdir = g_file_new_for_path (mpath);
		senum = g_file_enumerate_children (fsdir,
						   G_FILE_ATTRIBUTE_STANDARD_NAME,
						   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						   NULL,
						   &tmp_error);
		if (tmp_error != NULL)
			goto out;

		while ((s_finfo = g_file_enumerator_next_file (senum, NULL, &tmp_error)) != NULL) {
			g_autofree gchar *path = NULL;
			if (tmp_error != NULL)
				goto out;
			if (g_file_info_get_is_hidden (file_info))
				continue;

			path = g_build_filename (mpath,
						 g_file_info_get_name (s_finfo),
						 "control",
						 NULL);

			/* no control file means this is garbage, probably from a previous failed installation */
			if (!g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
				g_autofree gchar *tmp_path;
				tmp_path = g_build_filename (mpath,
								g_file_info_get_name (file_info),
								NULL);
				li_delete_dir_recursive (tmp_path);
			} else {
				child_count++;
			}
		}

		/* check if directory is empty, delete it in that case */
		if (child_count == 0)
			g_remove (mpath);
	}

out:
	if (tmp_error != NULL) {
		g_propagate_prefixed_error (error,
						tmp_error,
						"Error while cleaning up software directories:");
	}
}

/**
 * li_manager_cleanup:
 *
 * Remove unnecessary software.
 */
gboolean
li_manager_cleanup (LiManager *mgr, GError **error)
{
	g_autoptr(GHashTable) sws = NULL;
	g_autoptr(GHashTable) rts = NULL;
	g_autoptr(GList) sw_list = NULL;
	g_autoptr(GList) list = NULL;
	GPtrArray *rt_array;
	guint i;
	GList *l;
	GError *tmp_error = NULL;
	gboolean faded_sw_removed = FALSE;
	gboolean ret = FALSE;

	/* cleanup software directory */
	li_manager_cleanup_broken_packages (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	rts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	sws = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	/* load software list */
	sws = li_manager_get_installed_software (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}
	sw_list = g_hash_table_get_values (sws);

	/* first, always remove software which has the "faded" flag */
	for (l = sw_list; l != NULL; l = l->next) {
		LiPkgInfo *pki = LI_PKG_INFO (l->data);

		if (li_pkg_info_has_flag (pki, LI_PACKAGE_FLAG_FADED)) {
			g_debug ("Found faded package: %s", li_pkg_info_get_id (pki));
			li_manager_remove_software (mgr, li_pkg_info_get_id (pki), &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				goto out;
			}
			faded_sw_removed = TRUE;
			continue;
		}
	}

	/* update cache in case we removed software */
	if (faded_sw_removed) {
		/* installed software might have changed */
		li_manager_reset_cached_data (mgr);

		g_list_free (sw_list);
		g_hash_table_unref (sws);
		sws = li_manager_get_installed_software (mgr, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}

		sw_list = g_hash_table_get_values (sws);
	}

	/* remove every software from the list which is member of a runtime */
	rt_array = li_manager_get_installed_runtimes (mgr);
	for (i = 0; i < rt_array->len; i++) {
		gchar **rt_members;
		guint len;
		guint j;
		LiRuntime *rt = LI_RUNTIME (g_ptr_array_index (rt_array, i));

		rt_members = (gchar**) g_hash_table_get_keys_as_array (li_runtime_get_members (rt), &len);
		for (j = 0; j < len; j++) {
			g_hash_table_remove (sws, rt_members[j]);
		}
		g_free (rt_members);

		g_hash_table_insert (rts,
					g_strdup (li_runtime_get_uuid (rt)),
					g_object_ref (rt));
	}

	/* remove every software from the kill-list which references a valid runtime */
	list = g_hash_table_get_values (sws);
	for (l = list; l != NULL; l = l->next) {
		LiPkgInfo *pki = LI_PKG_INFO (l->data);

		if (g_hash_table_lookup (rts, li_pkg_info_get_runtime_dependency (pki)) != NULL) {
			g_hash_table_remove (sws, li_pkg_info_get_id (pki));
		}
	}
	g_list_free (list);

	list = g_hash_table_get_values (sws);
	for (l = list; l != NULL; l = l->next) {
		li_manager_remove_software (mgr, li_pkg_info_get_id (LI_PKG_INFO (l->data)), error);
		if (error != NULL)
			goto out;
	}

	/* cleanup tmp dir */
	li_delete_dir_recursive ("/var/tmp/limba");

	/* delete cleanup hint file, so the systemd service doesn't clean up if it is not necessary */
	if (g_file_test (LI_CLEANUP_HINT_FNAME, G_FILE_TEST_IS_REGULAR)) {
		if (g_remove (LI_CLEANUP_HINT_FNAME) != 0)
			g_warning ("Could no remove cleanup-hint (%s)", LI_CLEANUP_HINT_FNAME);
	}

	ret = TRUE;
out:
	/* installed software might have changed */
	li_manager_reset_cached_data (mgr);

	return ret;
}

/**
 * li_manager_refresh_cache:
 *
 * Refresh the cache of available packages.
 */
void
li_manager_refresh_cache (LiManager *mgr, GError **error)
{
	g_autoptr(LiPkgCache) cache = NULL;
	GError *tmp_error = NULL;

	cache = li_pkg_cache_new ();
	li_pkg_cache_open (cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	li_pkg_cache_update (cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}
}

/**
 * li_manager_receive_key:
 *
 * Download a PGP key and add it to the database of highly trusted keys.
 */
void
li_manager_receive_key (LiManager *mgr, const gchar *fpr, GError **error)
{
	LiKeyring *kr;

	kr = li_keyring_new ();
	li_keyring_import_key (kr, fpr, LI_KEYRING_KIND_USER, error);
	g_object_unref (kr);
}

/**
 * li_manager_clear_updates_table:
 */
static void
li_manager_clear_updates_table (LiManager *mgr)
{
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	/* clear table by recreating it, if necessary */
	if (g_hash_table_size (priv->updates) == 0)
		return;

	g_hash_table_unref (priv->updates);
	priv->updates = g_hash_table_new_full ((GHashFunc) li_pki_hash_func,
						(GEqualFunc) li_pki_equal_func,
						g_object_unref,
						g_object_unref);
}

/**
 * li_manager_get_update_list:
 *
 * EXPERIMENTAL
 *
 * Returns: (transfer full) (element-type LiUpdateItem): A list of #LiUpdateItem describing the potential updates. Free with g_list_free().
 **/
GList*
li_manager_get_update_list (LiManager *mgr, GError **error)
{
	g_autoptr(LiPkgCache) cache = NULL;
	g_autoptr(GHashTable) ipkgs = NULL;
	g_autoptr(GHashTable) apkgs = NULL;
	g_autoptr(GList) ipkg_list = NULL;
	GPtrArray *apkgs_list;
	guint i;
	GList *l;
	GError *tmp_error = NULL;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	cache = li_pkg_cache_new ();

	/* get a list of all packages we have installed */
	ipkgs = li_manager_get_installed_software (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	li_pkg_cache_open (cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	/* get available packages and prepare a hash map with the newest releases and the pkg name as key */
	apkgs_list = li_pkg_cache_get_packages (cache);
	apkgs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	for (i = 0; i < apkgs_list->len; i++) {
		LiPkgInfo *a_pki = NULL;
		LiPkgInfo *b_pki = LI_PKG_INFO (g_ptr_array_index (apkgs_list, i));

		a_pki = g_hash_table_lookup (apkgs, li_pkg_info_get_name (b_pki));
		if (a_pki != NULL) {
			if (li_compare_versions (li_pkg_info_get_version (a_pki), li_pkg_info_get_version (b_pki)) > 0) {
				/* the existing version is newer, we do not add it to the hash table */
				continue;
			}
		}

		g_hash_table_replace (apkgs,
					g_strdup (li_pkg_info_get_name (b_pki)),
					g_object_ref (b_pki));
	}

	/* ensure we have a clean updates table */
	li_manager_clear_updates_table (mgr);

	/* match it! */
	ipkg_list = g_hash_table_get_values (ipkgs);
	for (l = ipkg_list; l != NULL; l = l->next) {
		LiPkgInfo *ipki = LI_PKG_INFO (l->data);
		LiPkgInfo *apki = NULL;

		apki = g_hash_table_lookup (apkgs, li_pkg_info_get_name (ipki));
		/* check if we actually have a package available */
		if (apki == NULL)
			continue;

		if (li_compare_versions (li_pkg_info_get_version (apki), li_pkg_info_get_version (ipki)) > 0) {
			LiUpdateItem *uitem;

			/* we have a potential update */
			uitem = li_update_item_new_with_packages (ipki, apki);
			g_hash_table_insert (priv->updates,
						g_object_ref (ipki),
						uitem);
		}
	}

	return g_hash_table_get_values (priv->updates);
}

/**
 * li_manager_remove_exported_files_by_pki:
 */
static void
li_manager_remove_exported_files_by_pki (LiManager *mgr, LiPkgInfo *pki, GError **error)
{
	g_autofree gchar *swpath = NULL;
	g_autoptr(GFile) expfile = NULL;
	GError *tmp_error = NULL;
	gchar *tmp;

	swpath = g_build_filename (LI_SOFTWARE_ROOT,
					li_pkg_info_get_id (pki),
					NULL);

	/* delete exported files */
	/* FIXME: Move them away at first, to allow a later rollback */
	tmp = g_build_filename (swpath, "exported", NULL);
	expfile = g_file_new_for_path (tmp);
	g_free (tmp);
	if (g_file_query_exists (expfile, NULL)) {
		li_manager_remove_exported_files (expfile, &tmp_error);

		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}
		g_file_delete (expfile, NULL, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}
	}
}

/**
 * li_manager_upgrade_single_package:
 *
 * Upgrade a single package to a more recent version.
 */
static gboolean
li_manager_upgrade_single_package (LiManager *mgr, LiPkgInfo *ipki, LiPkgInfo *apki, GError **error)
{
	g_autoptr(LiInstaller) inst = NULL;
	GError *tmp_error = NULL;

	/* prepare installation */
	inst = li_installer_new ();
	li_installer_open_remote (inst, li_pkg_info_get_id (apki), &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	li_manager_remove_exported_files_by_pki (mgr, ipki, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* install new version */
	li_installer_install (inst, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	return TRUE;
}

/**
 * li_manager_apply_updates:
 *
 * EXPERIMENTAL
 */
gboolean
li_manager_apply_updates (LiManager *mgr, GError **error)
{
	g_autoptr(GList) updlist = NULL;
	g_autoptr(GHashTable) ipkgs = NULL;
	GList *l;
	GError *tmp_error = NULL;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	/* only search for new updates if we don't have some already in the queue */
	if (g_hash_table_size (priv->updates) == 0) {
		updlist = li_manager_get_update_list (mgr, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return FALSE;
		}
	} else {
		updlist = g_hash_table_get_values (priv->updates);
	}

	/* get a list of all packages we have installed */
	ipkgs = li_manager_get_installed_software (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	for (l = updlist; l != NULL; l = l->next) {
		LiPkgInfo *ipki;
		LiPkgInfo *apki;
		g_autoptr(GPtrArray) rts = NULL;

		LiUpdateItem *uitem = LI_UPDATE_ITEM (l->data);

		ipki = li_update_item_get_installed_pkg (uitem);
		apki = li_update_item_get_available_pkg (uitem);

		rts = li_manager_find_runtimes_with_member (mgr, ipki);
		if (rts == NULL) {
			/* we have no runtime, it is safe to update in any case */

			g_debug ("Performing straight-forward update of '%s'", li_pkg_info_get_id (ipki));

			li_manager_upgrade_single_package (mgr, ipki, apki, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return FALSE;
			}

			/* mark package as faded, so it will get killed on the shutdown-cleanup run */
			li_pkg_info_add_flag (ipki, LI_PACKAGE_FLAG_FADED);
			li_pkg_info_save_changes (ipki);

			/* tell the system to remove old packages on reboot */
			g_file_set_contents (LI_CLEANUP_HINT_FNAME, "please clean removed packages", -1, NULL);

		} else {
			guint i;
			g_autoptr(GPtrArray) update_rts = NULL;

			g_debug ("Performing complex upgrade of '%s'", li_pkg_info_get_id (ipki));

			update_rts = g_ptr_array_new_with_free_func (g_object_unref);
			for (i = 0; i < rts->len; i++) {
				gchar **reqs;
				guint j;
				LiRuntime *rt = LI_RUNTIME (g_ptr_array_index (rts, i));

				reqs = (gchar**) g_hash_table_get_keys_as_array (li_runtime_get_requirements (rt), NULL);
				for (j = 0; reqs[j] != NULL; j++) {
					LiPkgInfo *rt_req;
					rt_req = li_parse_dependency_string (reqs[i]);

					/* check if we can replace the package used by this runtime */
					if (li_pkg_info_satisfies_requirement (apki, rt_req)) {
						g_ptr_array_add (update_rts, g_object_ref (rt));
						break;
					}
				}

				g_free (reqs);
			}

			if (update_rts->len == 0) {
				/* we can't upgrade, the new version would break runtimes */
				g_debug ("Can not upgrade package '%s' as it would break all runtimes which are using it.", li_pkg_info_get_id (ipki));
				continue;
			}

			/* we can upgrade the package now! */
			li_manager_upgrade_single_package (mgr, ipki, apki, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_error (error, tmp_error);
				return FALSE;
			}

			for (i = 0; i < update_rts->len; i++) {
				LiRuntime *rt = LI_RUNTIME (g_ptr_array_index (update_rts, i));

				g_debug ("Updating runtime '%s'", li_runtime_get_uuid (rt));

				li_runtime_remove_package (rt, ipki);
				li_runtime_add_package (rt, apki);

				li_runtime_save (rt, &tmp_error);
				if (tmp_error != NULL) {
					g_propagate_error (error, tmp_error);
					return FALSE;
				}
			}

			/* tell the system to remove old packages on reboot */
			g_file_set_contents (LI_CLEANUP_HINT_FNAME, "please clean removed packages", -1, NULL);

			/* TODO:
			 *   - maybe explicitly flag ipki as crap if nothing uses it anymore?
			 */
		}
	}

	return TRUE;
}

/**
 * li_manager_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_manager_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiManagerError");
	return quark;
}

/**
 * li_manager_class_init:
 **/
static void
li_manager_class_init (LiManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_manager_finalize;

	signals[SIGNAL_PROGRESS] =
		g_signal_new ("progress",
				G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
				0, NULL, NULL, g_cclosure_marshal_VOID__UINT_POINTER,
				G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
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
