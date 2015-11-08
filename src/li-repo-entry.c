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
 * SECTION:li-repo-entry
 * @short_description: An entry in a Limba repository sources.list
 *
 */

#include "config.h"
#include "li-repo-entry.h"
#include "li-utils-private.h"

typedef struct _LiRepoEntryPrivate	LiRepoEntryPrivate;
struct _LiRepoEntryPrivate
{
	LiRepoEntryKinds kinds;
	gchar *url;

	gchar *md5sum;
	gchar *cache_dir;
	gchar *as_fname;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiRepoEntry, li_repo_entry, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_repo_entry_get_instance_private (o))

typedef struct {
	LiRepoEntry *cache;
	gchar *id;
} LiCacheProgressHelper;

/**
 * li_repo_entry_finalize:
 **/
static void
li_repo_entry_finalize (GObject *object)
{
	LiRepoEntry *re = LI_REPO_ENTRY (object);
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);

	g_free (priv->url);
	g_free (priv->md5sum);
	g_free (priv->cache_dir);
	g_free (priv->as_fname);

	G_OBJECT_CLASS (li_repo_entry_parent_class)->finalize (object);
}

/**
 * li_repo_entry_init:
 **/
static void
li_repo_entry_init (LiRepoEntry *re)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);

	priv->url = NULL;
	priv->kinds = LI_REPO_ENTRY_KIND_NONE;
}

/**
 * li_repo_entry_parse:
 *
 * Read a repository source entry in the form of "common,devel,source http://example.com/repo"
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
li_repo_entry_parse (LiRepoEntry *re, const gchar *repo_line)
{
	g_auto(GStrv) parts = NULL;
	g_auto(GStrv) kind_parts = NULL;
	guint i;
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);

	if (g_str_has_prefix (repo_line, "#")) {
		priv->kinds = LI_REPO_ENTRY_KIND_NONE;
		g_free (priv->url);
		priv->url = NULL;
		return TRUE;
	}

	parts = g_strsplit (repo_line, " ", 2);
	if (parts[0] == NULL)
		return FALSE;
	if (parts[1] == NULL)
		return FALSE;
	g_strstrip (parts[1]);

	/* determine which repo-source indices we should handle */
	kind_parts = g_strsplit (parts[0], ",", -1);
	for (i = 0; kind_parts[i] != NULL; i++) {
		LiRepoEntryKinds kind;
		g_strstrip (kind_parts[i]);

		kind = li_repo_entry_kind_from_string (kind_parts[i]);
		if (kind == LI_REPO_ENTRY_KIND_NONE) {
			g_warning ("Unknown source type '%s' for repository '%s'.", kind_parts[i], parts[1]);
			continue;
		}
		li_repo_entry_add_kind (re, kind);
	}

	/* add the URL for this repository */
	li_repo_entry_set_url (re, parts[1]);

	return TRUE;
}

/**
 * li_repo_entry_get_index_urls_for_arch:
 *
 * Get array of indices to download for the given architecture.
 */
gchar**
li_repo_entry_get_index_urls_for_arch (LiRepoEntry *re, const gchar *arch)
{
	g_autoptr(GPtrArray) urls = NULL;
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);

	/* check if the source is disabled, in that case we have nothing to download */
	if (li_repo_entry_has_kind (re, LI_REPO_ENTRY_KIND_NONE))
		return NULL;

	urls = g_ptr_array_new_with_free_func (g_free);

	if (li_repo_entry_has_kind (re, LI_REPO_ENTRY_KIND_COMMON))
		g_ptr_array_add (urls, g_build_filename (priv->url, "indices", arch, "Index.gz", NULL));

	if (li_repo_entry_has_kind (re, LI_REPO_ENTRY_KIND_DEVEL))
		g_ptr_array_add (urls, g_build_filename (priv->url, "indices", arch, "Index-Devel.gz", NULL));

	if (li_repo_entry_has_kind (re, LI_REPO_ENTRY_KIND_SOURCE))
		g_ptr_array_add (urls, g_build_filename (priv->url, "indices", arch, "Index-Sources.gz", NULL));

	return li_ptr_array_to_strv (urls);
}

/**
 * li_repo_entry_get_metadata_url_for_arch:
 *
 * Get AppStream metadata URL to download for the given architecture.
 */
gchar*
li_repo_entry_get_metadata_url_for_arch (LiRepoEntry *re, const gchar *arch)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);

	/* check if the source is disabled, in that case we have nothing to download */
	if (li_repo_entry_has_kind (re, LI_REPO_ENTRY_KIND_NONE))
		return NULL;

	/* SOURCE or DEVEL packages do not posses own AppStream metadata, and instead are linked to
	 * the existing metadata of their COMMON package, so we only have one URL here. */

	return g_build_filename (priv->url, "indices", arch, "Metadata.xml.gz", NULL);
}

/**
 * li_repo_entry_get_kinds:
 */
LiRepoEntryKinds
li_repo_entry_get_kinds (LiRepoEntry *re)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	return priv->kinds;
}

/**
 * li_repo_entry_set_kinds:
 */
void
li_repo_entry_set_kinds (LiRepoEntry *re, LiRepoEntryKinds kinds)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	priv->kinds = kinds;
}

/**
 * li_repo_entry_add_kind:
 */
void
li_repo_entry_add_kind (LiRepoEntry *re, LiRepoEntryKinds kind)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	priv->kinds |= kind;
}

/**
 * li_repo_entry_has_kind:
 */
gboolean
li_repo_entry_has_kind (LiRepoEntry *re, LiRepoEntryKinds kind)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	return priv->kinds & kind;
}

/**
 * li_repo_entry_get_url:
 */
const gchar*
li_repo_entry_get_url (LiRepoEntry *re)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	return priv->url;
}

/**
 * li_repo_entry_set_url:
 */
void
li_repo_entry_set_url (LiRepoEntry *re, const gchar *url)
{
	gchar *tmp;
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);

	g_free (priv->url);
	priv->url = g_strdup (url);

	/* ensure we have a unique identifier for this URL to be used in paths */
	/* FIXME: Take care of user adding more trailing slashes to the URL? */
	g_free (priv->md5sum);
	priv->md5sum = g_compute_checksum_for_string (G_CHECKSUM_MD5, priv->url, -1);

	/* set cache directory */
	g_free (priv->cache_dir);
	priv->cache_dir = g_build_filename (LIMBA_CACHE_DIR, priv->md5sum, NULL);
	g_mkdir_with_parents (priv->cache_dir, 0755);

	/* set AppStream target file */
	tmp = g_build_filename (APPSTREAM_CACHE_DIR, "xmls", NULL);
	g_mkdir_with_parents (tmp, 0755);
	g_free (tmp);
	tmp = g_strdup_printf ("limba_%s.xml.gz", priv->md5sum);
	priv->as_fname = g_build_filename (APPSTREAM_CACHE_DIR, "xmls", tmp, NULL);
	g_free (tmp);
}

/**
 * li_repo_entry_get_id:
 *
 * Returns: An unique identifier for this repository, to be used in filepaths.
 * Currently, that is the MD5 checksum of its URL.
 */
const gchar*
li_repo_entry_get_id (LiRepoEntry *re)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	return priv->md5sum;
}

/**
 * li_repo_entry_get_cache_dir:
 *
 * Returns: The private cache directory of this repository entry.
 */
const gchar*
li_repo_entry_get_cache_dir (LiRepoEntry *re)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	return priv->cache_dir;
}

/**
 * li_repo_entry_get_appstream_fname:
 *
 * Returns: The AppStream distro XML filename for this repository.
 */
const gchar*
li_repo_entry_get_appstream_fname (LiRepoEntry *re)
{
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	return priv->as_fname;
}

/**
 * li_repo_entry_kind_to_string:
 * @kind: the %LiRepoEntryKinds.
 *
 * Converts the flag value to an text representation.
 *
 * Returns: string version of @kind
 **/
const gchar*
li_repo_entry_kind_to_string (LiRepoEntryKinds kind)
{
	if (kind == LI_REPO_ENTRY_KIND_NONE)
		return "#";
	if (kind == LI_REPO_ENTRY_KIND_COMMON)
		return "common";
	if (kind == LI_REPO_ENTRY_KIND_DEVEL)
		return "devel";
	if (kind == LI_REPO_ENTRY_KIND_SOURCE)
		return "source";
	return NULL;
}

/**
 * li_repo_entry_kind_from_string:
 * @kind_str: the string.
 *
 * Converts the text representation to an flag value.
 *
 * Returns: a %LiRepoEntryKinds or %LI_REPO_ENTRY_KIND_NONE for unknown
 **/
LiRepoEntryKinds
li_repo_entry_kind_from_string (const gchar *kind_str)
{
	if (g_strcmp0 (kind_str, "common") == 0)
		return LI_REPO_ENTRY_KIND_COMMON;
	if (g_strcmp0 (kind_str, "devel") == 0)
		return LI_REPO_ENTRY_KIND_DEVEL;
	if (g_strcmp0 (kind_str, "source") == 0)
		return LI_REPO_ENTRY_KIND_SOURCE;
	return LI_REPO_ENTRY_KIND_NONE;
}

/**
 * li_repo_entry_class_init:
 **/
static void
li_repo_entry_class_init (LiRepoEntryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_repo_entry_finalize;
}

/**
 * li_repo_entry_new:
 *
 * Creates a new #LiRepoEntry.
 *
 * Returns: (transfer full): a #LiRepoEntry
 *
 **/
LiRepoEntry *
li_repo_entry_new (void)
{
	LiRepoEntry *re;
	re = g_object_new (LI_TYPE_REPO_ENTRY, NULL);
	return LI_REPO_ENTRY (re);
}
