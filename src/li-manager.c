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

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "li-utils.h"
#include "li-utils-private.h"
#include "li-pkg-info.h"
#include "li-pkg-cache.h"
#include "li-keyring.h"
#include "li-installer.h"

typedef struct _LiManagerPrivate	LiManagerPrivate;
struct _LiManagerPrivate
{
	GHashTable *pkgs; /* key:utf8;value:LiPkgInfo */
	GPtrArray *rts; /* of LiRuntime */
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

	g_hash_table_unref (priv->pkgs);
	g_ptr_array_unref (priv->rts);

	G_OBJECT_CLASS (li_manager_parent_class)->finalize (object);
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
	GFile *fdir;
	GFileEnumerator *enumerator = NULL;
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
		_cleanup_free_ gchar *path = NULL;
		if (tmp_error != NULL)
			goto out;

		if (g_file_info_get_is_hidden (file_info))
			continue;
		path = g_build_filename (LI_SOFTWARE_ROOT,
								 g_file_info_get_name (file_info),
								 "control",
								 NULL);
		if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			_cleanup_object_unref_ GFile *ctlfile = NULL;
			ctlfile = g_file_new_for_path (path);
			if (g_file_query_exists (ctlfile, NULL)) {
				LiPkgInfo *pki;

				/* create new LiPkgInfo for an installed package */
				pki = li_pkg_info_new ();
				li_pkg_info_add_flag (pki, LI_PACKAGE_FLAG_INSTALLED);

				li_pkg_info_load_file (pki, ctlfile, &tmp_error);
				if (tmp_error != NULL) {
					g_object_unref (pki);
					goto out;
				}

				/* do not list as installed if the software is faded */
				if (li_pkg_info_has_flag (pki, LI_PACKAGE_FLAG_FADED)) {
					g_object_unref (pki);
					continue;
				}

				g_hash_table_insert (pkgs,
									g_strdup (li_pkg_info_get_id (pki)),
									pki);
			}
		}
	}

out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
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
 * li_manager_get_software_list:
 *
 * Returns: (transfer container) (element-type LiPkgInfo): A list of installed and available software
 **/
GList*
li_manager_get_software_list (LiManager *mgr, GError **error)
{
	_cleanup_object_unref_ LiPkgCache *cache = NULL;
	GPtrArray *apkgs;
	GHashTable *ipkgs;
	guint i;
	GError *tmp_error = NULL;
	LiManagerPrivate *priv = GET_PRIVATE (mgr);

	if (g_hash_table_size (priv->pkgs) > 0) {
		/* we have cached data, so no need to search for it again */
		goto out;
	}

	cache = li_pkg_cache_new ();

	li_pkg_cache_open (cache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	/* populate table with installed packages */
	ipkgs = li_manager_get_installed_software (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
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

out:
	return g_hash_table_get_values (priv->pkgs);
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
	_cleanup_free_ gchar *runtime_root;
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
		gchar *path;
		if (tmp_error != NULL)
			goto out;

		if (g_file_info_get_is_hidden (file_info))
			continue;
		path = g_build_filename (runtime_root,
								 g_file_info_get_name (file_info),
								 "control",
								 NULL);
		if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			gchar *rt_path;
			gboolean ret;
			LiRuntime *rt;

			rt_path = g_build_filename (runtime_root, g_file_info_get_name (file_info), NULL);

			rt = li_runtime_new ();
			ret = li_runtime_load_directory (rt, rt_path, &tmp_error);
			if (ret)
				g_ptr_array_add (priv->rts, g_object_ref (rt));

			g_free (rt_path);
			g_object_unref (rt);
		}
		g_free (path);
	}


out:
	g_object_unref (fdir);
	if (enumerator != NULL)
		g_object_unref (enumerator);
	if (tmp_error != NULL) {
		g_printerr ("Error while searching for installed runtimes: %s\n", tmp_error->message);
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
		gchar **parts;
		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		parts = g_strsplit (line, "\t", 2);
		if (parts[1] == NULL) {
			g_strfreev (parts);
			continue;
		}

		if (g_str_has_prefix (parts[1], "/")) {
			/* delete file */
			res = g_remove (parts[1]);
			if (res != 0) {
				g_set_error (error,
					LI_MANAGER_ERROR,
					LI_MANAGER_ERROR_REMOVE_FAILED,
					_("Could not delete file '%s'"), parts[1]);
				g_strfreev (parts);
				goto out;
			}
		}
		g_strfreev (parts);
	}

out:
	g_object_unref (dis);
}

/**
 * li_manager_remove_software:
 **/
gboolean
li_manager_remove_software (LiManager *mgr, const gchar *pkgid, GError **error)
{
	_cleanup_free_ gchar *swpath = NULL;
	GFile *expfile;
	gchar *tmp;
	GError *tmp_error = NULL;
	_cleanup_ptrarray_unref_ GPtrArray *pkg_array = NULL;
	GFile *ctlfile;
	LiPkgInfo *pki;
	LiRuntime *rt;

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
		_cleanup_list_free_ GList *sw = NULL;
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
	_cleanup_free_ gchar *pkid = NULL;
	_cleanup_free_ gchar *path = NULL;

	pkid = g_strdup_printf ("%s-%s",
							li_pkg_info_get_name (pki), li_pkg_info_get_version (pki));
	path = g_build_filename (LI_SOFTWARE_ROOT, pkid, "control", NULL);

	if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
		return TRUE;

	return FALSE;
}

/**
 * li_manager_cleanup:
 *
 * Remove unnecessary software.
 */
gboolean
li_manager_cleanup (LiManager *mgr, GError **error)
{
	GHashTable *sws;
	GHashTable *rts;
	GPtrArray *rt_array;
	GList *sw_list = NULL;
	guint i;
	GList *l;
	GList *list = NULL;
	GError *tmp_error = NULL;
	gboolean ret = FALSE;

	rts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	sws = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	/* build hash tables */
	sw_list = li_manager_get_software_list (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	for (l = sw_list; l != NULL; l = l->next) {
		LiPkgInfo *pki = LI_PKG_INFO (l->data);

		/* check if the package is actually installed */
		if (!li_pkg_info_has_flag (pki, LI_PACKAGE_FLAG_INSTALLED))
			continue;

		g_hash_table_insert (sws,
							g_strdup (li_pkg_info_get_id (pki)),
							g_object_ref (pki));
	}

	/* remove every software from the list which is not member of a runtime */
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

		if (g_hash_table_lookup (rts, li_pkg_info_get_runtime_dependency (pki)) != NULL)
			g_hash_table_remove (sws, li_pkg_info_get_id (pki));
	}
	g_list_free (list);

	list = g_hash_table_get_values (sws);
	for (l = list; l != NULL; l = l->next) {
		li_manager_remove_software (mgr, li_pkg_info_get_id (LI_PKG_INFO (l->data)), error);
		if (error != NULL)
			goto out;
	}
	g_list_free (list);

	/* cleanup tmp dir */
	li_delete_dir_recursive ("/var/tmp/limba");

	/* installed software might have changed */
	li_manager_reset_cached_data (mgr);

	ret = TRUE;
out:
	if (list != NULL)
		g_list_free (list);
	g_list_free (sw_list);

	g_hash_table_unref (sws);
	g_hash_table_unref (rts);

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
	_cleanup_object_unref_ LiPkgCache *cache = NULL;
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
 * li_manager_get_update_list:
 *
 * EXPERIMENTAL
 **/
GHashTable*
li_manager_get_update_list (LiManager *mgr, GError **error)
{
	_cleanup_object_unref_ LiPkgCache *cache = NULL;
	GPtrArray *apkgs_list;
	_cleanup_hashtable_unref_ GHashTable *ipkgs = NULL;
	_cleanup_hashtable_unref_ GHashTable *apkgs = NULL;
	GHashTable *updates = NULL;
	guint i;
	_cleanup_list_free_ GList *ipkg_list = NULL;
	GList *l;
	GError *tmp_error = NULL;

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

	/* match it! */
	updates = g_hash_table_new_full ((GHashFunc) li_pki_hash_func,
									(GEqualFunc) li_pki_equal_func,
									g_object_unref,
									g_object_unref);
	ipkg_list = g_hash_table_get_values (ipkgs);
	for (l = ipkg_list; l != NULL; l = l->next) {
		LiPkgInfo *ipki = LI_PKG_INFO (l->data);
		LiPkgInfo *apki = NULL;

		apki = g_hash_table_lookup (apkgs, li_pkg_info_get_name (ipki));
		/* check if we actually have a package available */
		if (ipki == NULL)
			continue;

		if (li_compare_versions (li_pkg_info_get_version (apki), li_pkg_info_get_version (ipki)) > 0) {
			/* we have a potential update */
			g_hash_table_insert (updates,
							g_object_ref (ipki),
							g_object_ref (apki));
		}
	}

	return updates;
}

/**
 * li_manager_remove_exported_files_by_pki:
 */
static void
li_manager_remove_exported_files_by_pki (LiManager *mgr, LiPkgInfo *pki, GError **error)
{
	_cleanup_free_ gchar *swpath = NULL;
	_cleanup_object_unref_ GFile *expfile = NULL;
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
	_cleanup_object_unref_ LiInstaller *inst = NULL;
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
	_cleanup_hashtable_unref_ GHashTable *updates = NULL;
	_cleanup_hashtable_unref_ GHashTable *ipkgs = NULL;
	GHashTableIter iter;
	gpointer key, value;
	GError *tmp_error = NULL;

	updates = li_manager_get_update_list (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	/* get a list of all packages we have installed */
	ipkgs = li_manager_get_installed_software (mgr, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return FALSE;
	}

	g_hash_table_iter_init (&iter, updates);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		LiPkgInfo *ipki;
		LiPkgInfo *apki;
		_cleanup_ptrarray_unref_ GPtrArray *rts = NULL;
		ipki = LI_PKG_INFO (key);
		apki = LI_PKG_INFO (value);

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

			/* TODO: touch /var/lib/limba/cleanup-needed */

		} else {
			guint i;
			_cleanup_ptrarray_unref_ GPtrArray *recreate_rts = NULL;

			g_debug ("Performing complex upgrade of '%s'", li_pkg_info_get_id (ipki));

			recreate_rts = g_ptr_array_new_with_free_func (g_object_unref);
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
						g_ptr_array_add (recreate_rts, g_object_ref (rt));
						break;
					}
				}

				g_free (reqs);
			}

			if (recreate_rts->len == 0) {
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

			for (i = 0; i < recreate_rts->len; i++) {
				GHashTable *members_ht;
				gchar **members;
				_cleanup_ptrarray_unref_ GPtrArray *array = NULL;
				guint j;
				_cleanup_object_unref_ LiRuntime *new_rt = NULL;
				GList *ipkg_list;
				GList *l;
				LiRuntime *rt = LI_RUNTIME (g_ptr_array_index (recreate_rts, i));

				g_debug ("Replacing runtime '%s'", li_runtime_get_uuid (rt));

				members_ht = li_runtime_get_members (rt);

				/* replace is faster than g_strcmp-ing later, although this looks a bit uglier */
				g_hash_table_remove (members_ht, li_pkg_info_get_id (ipki));
				g_hash_table_add (members_ht, g_strdup (li_pkg_info_get_id (apki)));

				members = (gchar**) g_hash_table_get_keys_as_array (members_ht, NULL);
				array = g_ptr_array_new_with_free_func (g_object_unref);
				for (j = 0; members[j] != NULL; j++) {
					LiPkgInfo *pki;
					pki = li_pkg_info_new ();
					/* meh... */
					li_pkg_info_set_name (pki, members[j]);
					li_pkg_info_set_id (pki, members[j]);

					g_ptr_array_add (array, pki);
				}

				new_rt = li_runtime_create_with_members (array, &tmp_error);
				if (tmp_error != NULL) {
					g_propagate_error (error, tmp_error);
					return FALSE;
				}

				li_runtime_set_requirements (new_rt, li_runtime_get_requirements (rt));
				li_runtime_save (new_rt, &tmp_error);
				if (tmp_error != NULL) {
					g_propagate_error (error, tmp_error);
					return FALSE;
				}

				/* update runtime for dependent apps */
				ipkg_list = g_hash_table_get_values (ipkgs);
				for (l = ipkg_list; l != NULL; l = l->next) {
					LiPkgInfo *pki = LI_PKG_INFO (l->data);

					if (g_strcmp0 (li_pkg_info_get_runtime_dependency (pki), li_runtime_get_uuid (rt)) == 0) {
						li_pkg_info_set_runtime_dependency (pki, li_runtime_get_uuid (new_rt));
						li_pkg_info_save_changes (pki);
					}
				}

				/* destroy the old runtime */
				li_runtime_remove (rt);
			}

			/* TODO:
			 *   - touch /var/lib/limba/cleanup-needed
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
