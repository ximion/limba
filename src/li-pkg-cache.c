/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
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
 * SECTION:li-pkg-cache
 * @short_description: Download information about available packages from remote sources.
 *
 */

#include "config.h"
#include "li-pkg-cache.h"

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include <locale.h>
#include <gpgme.h>
#include <appstream.h>
#include <curl/curl.h>
#include <errno.h>

#include "li-utils.h"
#include "li-utils-private.h"
#include "li-pkg-index.h"
#include "li-keyring.h"

typedef struct _LiPkgCachePrivate	LiPkgCachePrivate;
struct _LiPkgCachePrivate
{
	LiPkgIndex *index;
	AsMetadata *metad;
	GPtrArray *repo_urls;

	LiKeyring *kr;
	gchar *cache_index_fname;
	gchar *tmp_dir;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPkgCache, li_pkg_cache, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_pkg_cache_get_instance_private (o))

#define LIMBA_CACHE_DIR "/var/cache/limba/"
#define APPSTREAM_CACHE "/var/cache/app-info/xmls"

/**
 * li_pkg_cache_finalize:
 **/
static void
li_pkg_cache_finalize (GObject *object)
{
	LiPkgCache *cache = LI_PKG_CACHE (object);
	LiPkgCachePrivate *priv = GET_PRIVATE (cache);

	g_object_unref (priv->index);
	g_object_unref (priv->metad);
	g_ptr_array_unref (priv->repo_urls);
	g_free (priv->cache_index_fname);
	g_object_unref (priv->kr);

	/* cleanup */
	li_delete_dir_recursive (priv->tmp_dir);
	g_free (priv->tmp_dir);

	G_OBJECT_CLASS (li_pkg_cache_parent_class)->finalize (object);
}

static void
li_pkg_cache_load_repolist (LiPkgCache *cache, const gchar *fname)
{
	gchar *content = NULL;
	gchar **lines;
	guint i;
	gboolean ret;
	LiPkgCachePrivate *priv = GET_PRIVATE (cache);

	/* Load repo list - failure to open it is no error, since it might simply be nonexistent.
	 * That could e.g. happen after a stateless system reset */
	ret = g_file_get_contents (fname, &content, NULL, NULL);
	if (!ret)
		return;

	lines = g_strsplit (content, "\n", -1);
	g_free (content);

	for (i = 0; lines[i] != NULL; i++) {
		g_strstrip (lines[i]);

		/* ignore comments */
		if (g_str_has_prefix (lines[i], "#"))
			continue;
		/* ignore empty lines */
		if (g_strcmp0 (lines[i], "") == 0)
			continue;

		g_ptr_array_add (priv->repo_urls, g_strdup (lines[i]));
	}

	g_strfreev (lines);
}

/**
 * li_pkg_cache_init:
 **/
static void
li_pkg_cache_init (LiPkgCache *cache)
{
	LiPkgCachePrivate *priv = GET_PRIVATE (cache);

	priv->index = li_pkg_index_new ();
	priv->metad = as_metadata_new ();
	priv->repo_urls = g_ptr_array_new_with_free_func (g_free);
	priv->cache_index_fname = g_build_filename (LIMBA_CACHE_DIR, "available.index", NULL);
	priv->kr = li_keyring_new ();

	/* get temporary directory */
	priv->tmp_dir = li_utils_get_tmp_dir ("remote");

	/* do not filter languages */
	as_metadata_set_locale (priv->metad, "ALL");

	/* load repository url list */
	li_pkg_cache_load_repolist (cache, "/etc/limba/sources.list"); /* defined by the user / distributor */
	li_pkg_cache_load_repolist (cache, "/var/lib/limba/update-sources.list"); /* managed automatically by Limba */
}

/**
 * curl_dl_write_data:
 */
static size_t
curl_dl_write_data (gpointer ptr, size_t size, size_t nmemb, FILE *stream)
{
	return fwrite (ptr, size, nmemb, stream);
}

/**
 * curl_dl_read_data:
 */
static size_t
curl_dl_read_data (gpointer ptr, size_t size, size_t nmemb, FILE *stream)
{
	return fread (ptr, size, nmemb, stream);
}

/**
 * li_pkg_cache_curl_progress_cb:
 */
gint
li_pkg_cache_curl_progress_cb (LiPkgCache *cache, double dltotal, double dlnow, double ultotal, double ulnow)
{
	/* TODO */
	return 0;
}

/**
 * li_pkg_cache_download_file_sync:
 */
static void
li_pkg_cache_download_file_sync (LiPkgCache *cache, const gchar *url, const gchar *dest, GError **error)
{
	CURL *curl;
	CURLcode res;
	FILE *outfile;
	long http_code = 0;

	curl = curl_easy_init();
	if (curl == NULL) {
		g_set_error (error,
				LI_PKG_CACHE_ERROR,
				LI_PKG_CACHE_ERROR_FAILED,
				_("Could not initialize CURL!"));
		return;
	}

	outfile = fopen (dest, "w");
	if (outfile == NULL) {
		g_set_error (error,
				LI_PKG_CACHE_ERROR,
				LI_PKG_CACHE_ERROR_FAILED,
				_("Could not open file '%s' for writing."), dest);

		curl_easy_cleanup (curl);
		return;
	}

	curl_easy_setopt (curl, CURLOPT_URL, url);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, outfile);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_dl_write_data);
	curl_easy_setopt (curl, CURLOPT_READFUNCTION, curl_dl_read_data);
	curl_easy_setopt (curl, CURLOPT_NOPROGRESS, FALSE);
	curl_easy_setopt (curl, CURLOPT_FAILONERROR, TRUE);
	curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, li_pkg_cache_curl_progress_cb);
	curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, cache);

	res = curl_easy_perform (curl);

	if (res != CURLE_OK) {
		curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code == 404) {
			/* TODO: Perform the same check for FTP? */
			g_set_error (error,
					LI_PKG_CACHE_ERROR,
					LI_PKG_CACHE_ERROR_REMOTE_NOT_FOUND,
					_("Could not find remote data '%s': %s."), url, curl_easy_strerror (res));
		} else {
			g_set_error (error,
					LI_PKG_CACHE_ERROR,
					LI_PKG_CACHE_ERROR_DOWNLOAD_FAILED,
					_("Unable to download data from '%s': %s."), url, curl_easy_strerror (res));
		}

		g_remove (dest);
	}

	/* cleanup */
	fclose (outfile);
	curl_easy_cleanup (curl);
}

/**
 * _li_pkg_cache_signature_hash_matches:
 *
 * Helper function to verify the repository signature
 */
gboolean
_li_pkg_cache_signature_hash_matches (gchar **sigparts, const gchar *fname, const gchar *id)
{
	gint i;
	gboolean valid;
	_cleanup_free_ gchar *hash = NULL;
	_cleanup_free_ gchar *expected_hash = NULL;

	for (i = 0; sigparts[i] != NULL; i++) {
		if (g_str_has_suffix (sigparts[i], id)) {
			gchar **tmp;
			tmp = g_strsplit (sigparts[i], "\t", 2);
			if (g_strv_length (tmp) != 2) {
				g_strfreev (tmp);
				continue;
			}
			expected_hash = g_strdup (tmp[0]);
			g_strfreev (tmp);
			break;
		}
	}

	hash = li_compute_checksum_for_file (fname);
	valid = g_strcmp0 (hash, expected_hash) == 0;
	if (!valid)
		g_debug ("Hash value of repository index '%s' do not match file.", id);

	return valid;
}

/**
 * li_pkg_cache_update:
 *
 * Update the package cache by downloading new package indices from the web.
 */
void
li_pkg_cache_update (LiPkgCache *cache, GError **error)
{
	guint i;
	GError *tmp_error = NULL;
	_cleanup_object_unref_ LiPkgIndex *tmp_index = NULL;
	_cleanup_object_unref_ LiPkgIndex *global_index = NULL;
	_cleanup_free_ gchar *dest_ascache = NULL;
	_cleanup_free_ gchar *current_arch = NULL;
	LiPkgCachePrivate *priv = GET_PRIVATE (cache);

	/* ensure AppStream cache exists */
	g_mkdir_with_parents (APPSTREAM_CACHE, 0755);

	/* create index of available packages */
	global_index = li_pkg_index_new ();

	/* clear AppStream metadata list */
	as_metadata_clear_components (priv->metad);
	dest_ascache = g_build_filename (APPSTREAM_CACHE, "limba-software.xml.gz", NULL);

	current_arch = li_get_current_arch_h ();

	for (i = 0; i < priv->repo_urls->len; i++) {
		const gchar *url;
		_cleanup_free_ gchar *url_index_all = NULL;
		_cleanup_free_ gchar *url_index_arch = NULL;
		_cleanup_free_ gchar *url_signature = NULL;
		_cleanup_free_ gchar *url_asdata = NULL;
		_cleanup_free_ gchar *dest = NULL;
		_cleanup_free_ gchar *dest_index_all = NULL;
		_cleanup_free_ gchar *dest_index_arch = NULL;
		_cleanup_free_ gchar *dest_asdata = NULL;
		_cleanup_free_ gchar *dest_repoconf = NULL;
		_cleanup_free_ gchar *dest_signature = NULL;
		_cleanup_object_unref_ GFile *idxfile = NULL;
		_cleanup_object_unref_ GFile *asfile = NULL;
		_cleanup_object_unref_ LiPkgIndex *tmp_index = NULL;
		_cleanup_strv_free_ gchar **hashlist = NULL;
		_cleanup_free_ gchar *fpr = NULL;
		gboolean index_read;
		GPtrArray *pkgs;
		guint j;
		gchar *tmp;
		gchar *tmp2;
		LiTrustLevel tlevel;
		url = (const gchar*) g_ptr_array_index (priv->repo_urls, i);

		/* create temporary index */
		tmp_index = li_pkg_index_new ();

		url_index_all = g_build_filename (url, "indices", "all", "Index.gz", NULL);
		url_index_arch = g_build_filename (url, "indices", current_arch, "Index.gz", NULL);
		url_asdata = g_build_filename (url, "indices", "Metadata.xml.gz", NULL);
		url_signature = g_build_filename (url, "indices", "Indices.gpg", NULL);

		/* prepare cache dir and ensure it exists */
		tmp = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
		dest = g_build_filename (LIMBA_CACHE_DIR, tmp, NULL);
		g_free (tmp);
		g_mkdir_with_parents (dest, 0755);

		dest_index_all = g_build_filename (dest, "Index-all.gz", NULL);
		dest_index_arch = g_strdup_printf ("%s/Index-%s.gz", dest, current_arch);
		dest_asdata = g_build_filename (dest, "Metadata.xml.gz", NULL);
		dest_signature = g_build_filename (dest, "Indices.gpg", NULL);

		g_debug ("Updating cached data for repository: %s", url);

		/* download indices */
		li_pkg_cache_download_file_sync (cache, url_index_all, dest_index_all, &tmp_error);
		if (tmp_error != NULL) {
			if (tmp_error->code == LI_PKG_CACHE_ERROR_REMOTE_NOT_FOUND) {
				/* we can ignore the error here, this repository is not for us */
				g_debug ("Skipping arch 'all' for repository: %s", url);
				g_error_free (tmp_error);
				tmp_error = NULL;
			} else {
				g_propagate_error (error, tmp_error);
				return;
			}
		}
		li_pkg_cache_download_file_sync (cache, url_index_arch, dest_index_arch, &tmp_error);
		if (tmp_error != NULL) {
			if (tmp_error->code == LI_PKG_CACHE_ERROR_REMOTE_NOT_FOUND) {
				/* we can ignore the error here, this repository is not for us */
				g_debug ("Skipping arch '%s' for repository: %s", current_arch, url);
				g_error_free (tmp_error);
				tmp_error = NULL;
			} else {
				g_propagate_error (error, tmp_error);
				return;
			}
		}

		/* download AppStream metadata */
		li_pkg_cache_download_file_sync (cache, url_asdata, dest_asdata, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}

		/* download signature */
		li_pkg_cache_download_file_sync (cache, url_signature, dest_signature, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}

		/* write repo hints file */
		dest_repoconf = g_build_filename (dest, "repo", NULL);
		g_file_set_contents (dest_repoconf, url, -1, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}

		g_debug ("Updated data for repository: %s", url);

		/* check signature */
		tmp = NULL;
		g_file_get_contents (dest_signature, &tmp, NULL, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_prefixed_error (error, tmp_error, "Unable to read signature data:");
			return;
		}
		tlevel = li_keyring_process_signature (priv->kr, tmp, &tmp2, &fpr, &tmp_error);
		g_free (tmp);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}
		hashlist = g_strsplit (tmp2, "\n", -1);
		g_free (tmp2);

		if (tlevel < LI_TRUST_LEVEL_MEDIUM) {
			g_set_error (error,
					LI_PKG_CACHE_ERROR,
					LI_PKG_CACHE_ERROR_VERIFICATION,
					_("Repository '%s' (signed with key '%s') is untrusted."), url, fpr);
			return;
		}

		/* load AppStream metadata */
		if (!_li_pkg_cache_signature_hash_matches (hashlist, dest_asdata, "indices/Metadata.xml.gz")) {
			g_set_error (error,
					LI_PKG_CACHE_ERROR,
					LI_PKG_CACHE_ERROR_VERIFICATION,
					_("Siganture on '%s' is invalid."), url);
			return;
		}
		asfile = g_file_new_for_path (dest_asdata);
		as_metadata_parse_file (priv->metad, asfile, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_prefixed_error (error, tmp_error, "Unable to load AppStream data for: %s", url);
			return;
		}

		/* load index */
		idxfile = g_file_new_for_path (dest_index_all);
		if (g_file_query_exists (idxfile, NULL)) {
			/* validate */
			if (!_li_pkg_cache_signature_hash_matches (hashlist, dest_index_all, "indices/all/Index.gz")) {
				g_set_error (error,
						LI_PKG_CACHE_ERROR,
						LI_PKG_CACHE_ERROR_VERIFICATION,
						_("Siganture on '%s' is invalid."), url);
				return;
			}

			li_pkg_index_load_file (tmp_index, idxfile, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_prefixed_error (error, tmp_error, "Unable to load index for repository: %s", url);
				return;
			}
			index_read = TRUE;
		}

		g_object_unref (idxfile);
		idxfile = g_file_new_for_path (dest_index_arch);
		if (g_file_query_exists (idxfile, NULL)) {
			/* validate */
			tmp = g_strdup_printf ("indices/%s/Index.gz", current_arch);
			if (!_li_pkg_cache_signature_hash_matches (hashlist, dest_index_arch, tmp)) {
				g_free (tmp);
				g_set_error (error,
						LI_PKG_CACHE_ERROR,
						LI_PKG_CACHE_ERROR_VERIFICATION,
						_("Siganture on '%s' is invalid."), url);
				return;
			}
			g_free (tmp);

			li_pkg_index_load_file (tmp_index, idxfile, &tmp_error);
			if (tmp_error != NULL) {
				g_propagate_prefixed_error (error, tmp_error, "Unable to load index for repository: %s", url);
				return;
			}
			index_read = TRUE;
		}

		if (!index_read)
			g_warning ("Repository '%s' does not seem to contain any index file!", url);

		/* ensure that all locations are set properly */
		pkgs = li_pkg_index_get_packages (tmp_index);
		for (j = 0; j < pkgs->len; j++) {
			_cleanup_free_ gchar *new_url = NULL;
			LiPkgInfo *pki = LI_PKG_INFO (g_ptr_array_index (pkgs, j));

			/* mark package as available for installation */
			li_pkg_info_add_flag (pki, LI_PACKAGE_FLAG_AVAILABLE);

			new_url = g_build_filename (url, li_pkg_info_get_repo_location (pki), NULL);
			li_pkg_info_set_repo_location (pki, new_url);

			/* add to pkg cache */
			li_pkg_index_add_package (global_index, pki);
		}


		g_debug ("Loaded index of repository.");
	}

	/* save AppStream XML data */
	as_metadata_save_distro_xml (priv->metad, dest_ascache, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_prefixed_error (error, tmp_error, "Unable to save metadata.");
		return;
	}

	/* save global index file */
	li_pkg_index_save_to_file (global_index, priv->cache_index_fname);
}

/**
 * li_pkg_cache_open:
 *
 * Open the package cache and load a list of available packages.
 */
void
li_pkg_cache_open (LiPkgCache *cache, GError **error)
{
	GError *tmp_error = NULL;
	_cleanup_object_unref_ GFile *file = NULL;
	LiPkgCachePrivate *priv = GET_PRIVATE (cache);

	/* TODO: Implement li_pkg_index_clear() */
	g_object_unref (priv->index);
	priv->index = li_pkg_index_new ();

	file = g_file_new_for_path (priv->cache_index_fname);

	if (g_file_query_exists (file, NULL)) {
		li_pkg_index_load_file (priv->index, file, &tmp_error);
	}
	if (tmp_error != NULL) {
		g_propagate_prefixed_error (error, tmp_error, "Unable to load package cache: ");
		return;
	}
}

/**
 * li_pkg_cache_get_packages:
 *
 * Returns: (transfer none) (element-type LiPkgInfo): Packages in the index
 */
GPtrArray*
li_pkg_cache_get_packages (LiPkgCache *cache)
{
	LiPkgCachePrivate *priv = GET_PRIVATE (cache);
	return li_pkg_index_get_packages (priv->index);
}

/**
 * li_pkg_cache_fetch_remote:
 * @cache: an instance of #LiPkgCache
 * @pkgid: id of the package to download
 *
 * Download and open a package from a remote source.
 *
 * Returns: (transfer full): A new #LiPackage, or %NULL in case of an error
 */
LiPackage*
li_pkg_cache_fetch_remote (LiPkgCache *cache, const gchar *pkgid, GError **error)
{
	GPtrArray *pkgs;
	GError *tmp_error = NULL;
	guint i;
	LiPkgInfo *pki = NULL;
	gchar *tmp;
	_cleanup_free_ gchar *dest_fname = NULL;
	LiPackage *pkg;
	LiPkgCachePrivate *priv = GET_PRIVATE (cache);

	/* find our package metadata */
	pkgs = li_pkg_cache_get_packages (cache);
	for (i = 0; i < pkgs->len; i++) {
		LiPkgInfo *tmp_pki = LI_PKG_INFO (g_ptr_array_index (pkgs, i));
		if (g_strcmp0 (li_pkg_info_get_id (tmp_pki), pkgid) == 0) {
			pki = tmp_pki;
			break;
		}
	}

	if (pki == NULL) {
		g_set_error (error,
				LI_PKG_CACHE_ERROR,
				LI_PKG_CACHE_ERROR_NOT_FOUND,
				_("Could not find package matching id '%s'."), pkgid);
		return NULL;
	}

	tmp = g_path_get_basename (li_pkg_info_get_repo_location (pki));
	dest_fname = g_build_filename (priv->tmp_dir, tmp, NULL);
	g_free (tmp);

	g_debug ("Fetching remote package from: %s", li_pkg_info_get_repo_location (pki));
	li_pkg_cache_download_file_sync (cache,
								li_pkg_info_get_repo_location (pki),
								dest_fname,
								&tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}
	g_debug ("Package '%s' downloaded from remote.", pkgid);

	pkg = li_package_new ();
	li_package_open_file (pkg, dest_fname, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	return pkg;
}

/**
 * li_pkg_cache_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_pkg_cache_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiPkgCacheError");
	return quark;
}

/**
 * li_pkg_cache_class_init:
 **/
static void
li_pkg_cache_class_init (LiPkgCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_pkg_cache_finalize;
}

/**
 * li_pkg_cache_new:
 *
 * Creates a new #LiPkgCache.
 *
 * Returns: (transfer full): a #LiPkgCache
 *
 **/
LiPkgCache *
li_pkg_cache_new (void)
{
	LiPkgCache *cache;
	cache = g_object_new (LI_TYPE_PKG_CACHE, NULL);
	return LI_PKG_CACHE (cache);
}
