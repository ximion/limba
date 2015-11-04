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

typedef struct _LiRepoEntryPrivate	LiRepoEntryPrivate;
struct _LiRepoEntryPrivate
{
	LiRepoEntryKinds kinds;
	gchar *url;
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
	LiRepoEntryPrivate *priv = GET_PRIVATE (re);
	g_free (priv->url);
	priv->url = g_strdup (url);
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
