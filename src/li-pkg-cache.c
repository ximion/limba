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
 * SECTION:li-repository
 * @short_description: A local Limba package repository
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

#include "li-utils-private.h"
#include "li-pkg-index.h"

typedef struct _LiPkgCachePrivate	LiPkgCachePrivate;
struct _LiPkgCachePrivate
{
	LiPkgIndex *index;
	AsMetadata *metad;
	gchar *repo_path;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiPkgCache, li_pkg_cache, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_pkg_cache_get_instance_private (o))

/**
 * li_pkg_cache_finalize:
 **/
static void
li_pkg_cache_finalize (GObject *object)
{
	LiPkgCache *pkgcache = LI_PKG_CACHE (object);
	LiPkgCachePrivate *priv = GET_PRIVATE (pkgcache);

	g_free (priv->repo_path);
	g_object_unref (priv->index);
	g_object_unref (priv->metad);

	G_OBJECT_CLASS (li_pkg_cache_parent_class)->finalize (object);
}

/**
 * li_pkg_cache_init:
 **/
static void
li_pkg_cache_init (LiPkgCache *pkgcache)
{
	LiPkgCachePrivate *priv = GET_PRIVATE (pkgcache);

	priv->index = li_pkg_index_new ();
	priv->metad = as_metadata_new ();
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
li_pkg_cache_curl_progress_cb (LiPkgCache *pkgcache, double dltotal, double dlnow, double ultotal, double ulnow)
{
	/* TODO */
	return 0;
}

/**
 * li_pkg_cache_download_thread:
 */
static gpointer
li_pkg_cache_download_file_sync (LiPkgCache *pkgcache, const gchar *url, const gchar *dest, GError **error)
{
	CURL *curl;
	CURLcode res;
	FILE *outfile;

	curl = curl_easy_init();
	if (curl == NULL) {
		g_warning ("Could not initialize CURL!");
		return NULL;
	}

	outfile = fopen (dest, "w");

	curl_easy_setopt (curl, CURLOPT_URL, url);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, outfile);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_dl_write_data);
	curl_easy_setopt (curl, CURLOPT_READFUNCTION, curl_dl_read_data);
	curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, li_pkg_cache_curl_progress_cb);
	curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, pkgcache);

	res = curl_easy_perform (curl);

	if (res != CURLE_OK) {
		g_set_error (error,
				LI_PKG_CACHE_ERROR,
				LI_PKG_CACHE_ERROR_DOWNLOAD_FAILED,
				_("Unable to download data from '%s': %s."), url, curl_easy_strerror (res));
	}

	/* cleanup */
	fclose (outfile);
	curl_easy_cleanup(curl);

	return NULL;
}

/**
 * li_pkg_cache_update:
 *
 * Update the package cache by downloading new package indices from the web.
 */
void
li_pkg_cache_update (LiPkgCache *pkgcache, GError **error)
{
	/* TODO */

	li_pkg_cache_download_file_sync (pkgcache, "http://imgs.xkcd.com/comics/location_sharing.png", "/tmp/xkcd.png", error);
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
	LiPkgCache *pkgcache;
	pkgcache = g_object_new (LI_TYPE_PKG_CACHE, NULL);
	return LI_PKG_CACHE (pkgcache);
}
