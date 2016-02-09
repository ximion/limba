/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015-2016 Matthias Klumpp <matthias@tenstral.net>
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
 * SECTION:li-build-conf
 * @short_description: Read YAML data storing information how to build a project.
 */

#include "li-build-conf.h"

#include <config.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <yaml.h>
#include <appstream.h>

#include "limba.h"
#include "li-config-data.h"
#include "li-utils-private.h"

typedef struct _LiBuildConfPrivate	LiBuildConfPrivate;
struct _LiBuildConfPrivate
{
	GNode *yroot;
	LiPkgInfo *pki;
	gchar *extra_bundles_dir;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiBuildConf, li_build_conf, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_build_conf_get_instance_private (o))

enum YamlNodeKind {
	YAML_VAR,
	YAML_VAL,
	YAML_SEQ
};

static void li_build_conf_free_doctree (GNode *root);

/**
 * li_build_conf_finalize:
 **/
static void
li_build_conf_finalize (GObject *object)
{
	LiBuildConf *bconf = LI_BUILD_CONF (object);
	LiBuildConfPrivate *priv = GET_PRIVATE (bconf);

	li_build_conf_free_doctree (priv->yroot);
	if (priv->pki != NULL)
		g_object_unref (priv->pki);
	g_free (priv->extra_bundles_dir);

	G_OBJECT_CLASS (li_build_conf_parent_class)->finalize (object);
}

/**
 * li_build_conf_init:
 **/
static void
li_build_conf_init (LiBuildConf *bconf)
{
	LiBuildConfPrivate *priv = GET_PRIVATE (bconf);

	priv->yroot = NULL;
}

/**
 * li_build_conf_get_root_node:
 */
static GNode*
li_build_conf_get_root_node (LiBuildConf *bconf, const gchar *node_name)
{
	GNode *node;
	LiBuildConfPrivate *priv = GET_PRIVATE (bconf);

	for (node = priv->yroot->children; node != NULL; node = node->next) {
		gchar *key;

		key = (gchar*) node->data;

		if (g_strcmp0 (key, node_name) == 0) {
			return node;
		}
	}

	return NULL;
}

/**
 * li_build_conf_get_before_script:
 */
GPtrArray*
li_build_conf_get_before_script (LiBuildConf *bconf)
{
	GPtrArray *cmds;
	GNode *root;
	GNode *n;

	root = li_build_conf_get_root_node (bconf, "before_script");
	if (root == NULL)
		return NULL;

	cmds = g_ptr_array_new_with_free_func (g_free);

	for (n = root->children; n != NULL; n = n->next) {
		g_ptr_array_add (cmds,
				g_strdup ((gchar*) n->data));
	}

	return cmds;
}

/**
 * li_build_conf_get_script:
 */
GPtrArray*
li_build_conf_get_script (LiBuildConf *bconf)
{
	GPtrArray *cmds;
	GNode *root;
	GNode *n;

	root = li_build_conf_get_root_node (bconf, "script");
	if (root == NULL)
		return NULL;

	cmds = g_ptr_array_new_with_free_func (g_free);

	for (n = root->children; n != NULL; n = n->next) {
		g_ptr_array_add (cmds,
				g_strdup ((gchar*) n->data));
	}

	return cmds;
}

/**
 * li_build_conf_get_after_script:
 */
GPtrArray*
li_build_conf_get_after_script (LiBuildConf *bconf)
{
	GPtrArray *cmds;
	GNode *root;
	GNode *n;

	root = li_build_conf_get_root_node (bconf, "after_script");
	if (root == NULL)
		return NULL;

	cmds = g_ptr_array_new_with_free_func (g_free);

	for (n = root->children; n != NULL; n = n->next) {
		g_ptr_array_add (cmds,
				g_strdup ((gchar*) n->data));
	}

	return cmds;
}

/**
 * li_build_conf_get_pkginfo:
 *
 * Returns: (transfer full): LiPkgInfo
 */
LiPkgInfo*
li_build_conf_get_pkginfo (LiBuildConf *bconf)
{
	LiBuildConfPrivate *priv = GET_PRIVATE (bconf);
	return g_object_ref (priv->pki);
}

/**
 * li_build_conf_get_extra_bundles_dir:
 *
 * Returns: (transfer full): Path to extra bundles.
 */
gchar*
li_build_conf_get_extra_bundles_dir (LiBuildConf *bconf)
{
	LiBuildConfPrivate *priv = GET_PRIVATE (bconf);
	return g_strdup (priv->extra_bundles_dir);
}

/**
 * _buildconf_yaml_free_node:
 */
static gboolean
_buildconf_yaml_free_node (GNode *node, gpointer data)
{
	if (node->data != NULL)
		g_free (node->data);

	return FALSE;
}

/**
 * li_build_conf_free_doctree:
 */
static void
li_build_conf_free_doctree (GNode *root)
{
	if (root == NULL)
		return;

	g_node_traverse (root,
			G_IN_ORDER,
			G_TRAVERSE_ALL,
			-1,
			_buildconf_yaml_free_node,
			NULL);
	g_node_destroy (root);
}

/**
 * li_build_conf_yaml_process_layer:
 *
 * Create GNode tree from DEP-11 YAML document
 */
static void
build_conf_yaml_process_layer (yaml_parser_t *parser, GNode *data)
{
	GNode *last_leaf = data;
	GNode *last_scalar;
	yaml_event_t event;
	gboolean parse = TRUE;
	gboolean in_sequence = FALSE;
	int storage = YAML_VAR; /* the first element must always be of type VAR */

	while (parse) {
		yaml_parser_parse (parser, &event);

		/* Parse value either as a new leaf in the mapping
		 * or as a leaf value (one of them, in case it's a sequence) */
		switch (event.type) {
			case YAML_SCALAR_EVENT:
				if (storage)
					g_node_append_data (last_leaf, g_strdup ((gchar*) event.data.scalar.value));
				else
					last_leaf = g_node_append (data, g_node_new (g_strdup ((gchar*) event.data.scalar.value)));
				storage ^= YAML_VAL;
				break;
			case YAML_SEQUENCE_START_EVENT:
				storage = YAML_SEQ;
				in_sequence = TRUE;
				break;
			case YAML_SEQUENCE_END_EVENT:
				storage = YAML_VAR;
				in_sequence = FALSE;
				break;
			case YAML_MAPPING_START_EVENT:
				/* depth += 1 */
				last_scalar = last_leaf;
				if (in_sequence)
					last_leaf = g_node_append (last_leaf, g_node_new (g_strdup ("-")));
				build_conf_yaml_process_layer (parser, last_leaf);
				last_leaf = last_scalar;
				storage ^= YAML_VAL; /* Flip VAR/VAL, without touching SEQ */
				break;
			case YAML_MAPPING_END_EVENT:
			case YAML_STREAM_END_EVENT:
			case YAML_DOCUMENT_END_EVENT:
				/* depth -= 1 */
				parse = FALSE;
				break;
			default:
				break;
		}

		yaml_event_delete (&event);
	}
}

/**
 * li_build_conf_process_data:
 */
void
li_build_conf_process_data (LiBuildConf *bconf, const gchar *data, GError **error)
{
	yaml_parser_t parser;
	yaml_event_t event;
	gboolean parse = TRUE;
	LiBuildConfPrivate *priv = GET_PRIVATE (bconf);

	yaml_parser_initialize (&parser);
	yaml_parser_set_input_string (&parser, (unsigned char*) data, strlen(data));

	/* ensure we empty the document */
	li_build_conf_free_doctree (priv->yroot);
	priv->yroot = NULL;

	while (parse) {
		yaml_parser_parse(&parser, &event);
		if (event.type == YAML_DOCUMENT_START_EVENT) {
			GNode *root = g_node_new (g_strdup (""));

			build_conf_yaml_process_layer (&parser, root);

			priv->yroot = root;
		}

		/* stop if end of stream is reached */
		if (event.type == YAML_STREAM_END_EVENT)
			parse = FALSE;

		yaml_event_delete(&event);
	}

	yaml_parser_delete (&parser);
}

/**
 * li_build_conf_open_file:
 **/
void
li_build_conf_open_file (LiBuildConf *bconf, GFile* file, GError **error)
{
	gchar *yaml_doc;
	GFileInputStream* fistream;
	gchar *line = NULL;
	GString *str;
	GDataInputStream *dis;

	str = g_string_new ("");
	fistream = g_file_read (file, NULL, NULL);
	dis = g_data_input_stream_new ((GInputStream*) fistream);
	g_object_unref (fistream);

	while (TRUE) {
		line = g_data_input_stream_read_line (dis, NULL, NULL, NULL);
		if (line == NULL) {
			break;
		}

		g_string_append_printf (str, "%s\n", line);
	}

	yaml_doc = g_string_free (str, FALSE);
	g_object_unref (dis);

	/* parse YAML data */
	li_build_conf_process_data (bconf, yaml_doc, error);
	g_free (yaml_doc);
}


/**
 * li_build_conf_open_from_dir:
 */
void
li_build_conf_open_from_dir (LiBuildConf *bconf, const gchar *dir, GError **error)
{
	GFile *file;
	GError *tmp_error = NULL;
	gchar *fname = NULL;
	AsComponent *cpt;
	gchar *tmp;
	const gchar *version;
	g_autoptr(LiPkgInfo) pki = NULL;
	g_autoptr(LiConfigData) cdata = NULL;
	g_autoptr(AsMetadata) mdata = NULL;
	LiBuildConfPrivate *priv = GET_PRIVATE (bconf);

	fname = g_build_filename (dir, "lipkg", "build.yml", NULL);
	file = g_file_new_for_path (fname);
	if (!g_file_query_exists (file, NULL)) {
		g_object_unref (file);
		g_free (fname);
		fname = NULL;
	}
	if (fname == NULL) {
		fname = g_build_filename (dir, "build.yml", NULL);
		file = g_file_new_for_path (fname);
		if (!g_file_query_exists (file, NULL)) {
			g_object_unref (file);
			g_free (fname);
			fname = NULL;
		}
	}
	if (fname == NULL) {
		fname = g_build_filename (dir, ".travis.yml", NULL);
		file = g_file_new_for_path (fname);
		if (!g_file_query_exists (file, NULL)) {
			g_object_unref (file);
			g_free (fname);
			fname = NULL;
		}
	}
	if (fname == NULL) {
		/* looks like we didn't find a file */
		g_set_error_literal (error,
					G_FILE_ERROR,
					G_FILE_ERROR_NOENT,
					_("Could not find a 'build.yml' file!"));
		return;
	}

	li_build_conf_open_file (bconf, file, error);
	g_free (fname);
	g_object_unref (file);

	/* get list of build dependencies */
	fname = g_build_filename (dir, "lipkg", "control", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);
	if (!g_file_query_exists (file, NULL)) {
		g_object_unref (file);
		g_set_error_literal (error,
					G_FILE_ERROR,
					G_FILE_ERROR_NOENT,
					_("Could not find an IPK control file!"));
		return;
	}

	/* get basic package info */
	pki = li_pkg_info_new ();
	li_pkg_info_load_file (pki, file, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_object_unref (file);
		return;
	}

	/* read some extra fields relevant for building only */
	cdata = li_config_data_new ();
	li_config_data_load_file (cdata, file, &tmp_error);
	g_object_unref (file);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		g_object_unref (file);
		return;
	}
	priv->extra_bundles_dir = li_config_data_get_value (cdata, "ExtraBundlesDir");

	/* load elementary information from AppStream metainfo */
	fname = g_build_filename (dir, "lipkg", "metainfo.xml", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);
	if (!g_file_query_exists (file, NULL)) {
		g_object_unref (file);
		g_set_error_literal (error,
					G_FILE_ERROR,
					G_FILE_ERROR_NOENT,
					_("Could not find an AppStream metainfo file!"));
		return;
	}

	mdata = as_metadata_new ();
	as_metadata_set_locale (mdata, "C");
	as_metadata_parse_file (mdata, file, &tmp_error);
	g_object_unref (file);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}
	cpt = as_metadata_get_component (mdata);

	/* set basic package information */
	tmp = li_get_pkgname_from_component (cpt);
	if (tmp == NULL) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Could not determine package name."));
		return;
	}
	li_pkg_info_set_name (pki, tmp);
	g_free (tmp);

	version = li_get_last_version_from_component (cpt);
	if (version == NULL) {
		/* no version? give up. */
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_DATA_MISSING,
				_("Could not determine package version."));
		return;
	}
	li_pkg_info_set_version (pki, version);

	/* the human-friendly application name */
	li_pkg_info_set_appname (pki, as_component_get_name (cpt));

	priv->pki = g_object_ref (pki);
}

/**
 * li_build_conf_class_init:
 **/
static void
li_build_conf_class_init (LiBuildConfClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_build_conf_finalize;
}

/**
 * li_build_conf_new:
 *
 * Creates a new #LiBuildConf.
 *
 * Returns: (transfer full): a #LiBuildConf
 *
 **/
LiBuildConf *
li_build_conf_new (void)
{
	LiBuildConf *bconf;
	bconf = g_object_new (LI_TYPE_BUILD_CONF, NULL);
	return LI_BUILD_CONF (bconf);
}
