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

#include "li-utils-private.h"
#include "li-pkg-index.h"

typedef struct _LiPkgCachePrivate	LiPkgCachePrivate;
struct _LiPkgCachePrivate
{
	LiPkgIndex *index;
	AsMetadata *metad;

	GPtrArray *repo_urls;
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
		g_set_error (error,
				LI_PKG_CACHE_ERROR,
				LI_PKG_CACHE_ERROR_DOWNLOAD_FAILED,
				_("Unable to download data from '%s': %s."), url, curl_easy_strerror (res));
	}

	/* cleanup */
	fclose (outfile);
	curl_easy_cleanup (curl);
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
	LiPkgCachePrivate *priv = GET_PRIVATE (cache);

	/* ensure AppStream cache exists */
	g_mkdir_with_parents (APPSTREAM_CACHE, 0755);

	for (i = 0; i < priv->repo_urls->len; i++) {
		const gchar *url;
		gint res;
		_cleanup_free_ gchar *url_index = NULL;
		_cleanup_free_ gchar *url_asdata = NULL;
		_cleanup_free_ gchar *dest = NULL;
		_cleanup_free_ gchar *dest_index = NULL;
		_cleanup_free_ gchar *dest_asdata = NULL;
		_cleanup_free_ gchar *dest_ascache = NULL;
		gchar *tmp;
		url = (const gchar*) g_ptr_array_index (priv->repo_urls, i);

		url_index = g_build_filename (url, "Index.gz", NULL);
		url_asdata = g_build_filename (url, "Metadata.xml.gz", NULL);

		/* prepare cache dir and ensure it exists */
		tmp = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
		dest = g_build_filename (LIMBA_CACHE_DIR, tmp, NULL);
		dest_ascache = g_strdup_printf ("%s/limba-%s.xml.gz", APPSTREAM_CACHE, tmp);
		g_free (tmp);
		g_mkdir_with_parents (dest, 0755);

		dest_index = g_build_filename (dest, "Index.gz", NULL);
		dest_asdata = g_build_filename (dest, "Metadata.xml.gz", NULL);

		g_debug ("Updating cached data for repository: %s", url_index);
		li_pkg_cache_download_file_sync (cache, url_index, dest_index, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}

		li_pkg_cache_download_file_sync (cache, url_asdata, dest_asdata, &tmp_error);
		if (tmp_error != NULL) {
			g_propagate_error (error, tmp_error);
			return;
		}

		/* create symbolic link in the AppStream cache, to make data known to SCs */
		res = symlink (dest_asdata, dest_ascache);
		if ((res != 0) && (res != EEXIST)) {
			g_set_error (error,
				LI_PKG_CACHE_ERROR,
				LI_PKG_CACHE_ERROR_WRITE,
				_("Unable to write symbolic link to AppStream cache: %s."), g_strerror (errno));
		}

		g_debug ("Updated data for repository: %s", url_index);
	}
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
