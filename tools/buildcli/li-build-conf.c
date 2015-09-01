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
 * SECTION:li-build-conf
 * @short_description: Read YAML data storing information how to build a project.
 */

#include "li-build-conf.h"

#include <glib.h>
#include <glib-object.h>
#include <yaml.h>

typedef struct _LiBuildConfPrivate	LiBuildConfPrivate;
struct _LiBuildConfPrivate
{
	GNode *yroot;
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
	LiBuildConf *cdata;
	cdata = g_object_new (LI_TYPE_BUILD_CONF, NULL);
	return LI_BUILD_CONF (cdata);
}
