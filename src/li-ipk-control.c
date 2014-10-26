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
 * SECTION:li-ipk-control
 * @short_description: Control metadata for IPK packages
 */

#include "config.h"
#include "li-ipk-control.h"
#include "li-config-data.h"

typedef struct _LiIPKControlPrivate	LiIPKControlPrivate;
struct _LiIPKControlPrivate
{
	LiConfigData *cdata;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiIPKControl, li_ipk_control, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_ipk_control_get_instance_private (o))

/**
 * li_ipk_control_finalize:
 **/
static void
li_ipk_control_finalize (GObject *object)
{
	LiIPKControl *ipkc = LI_IPK_CONTROL (object);
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);

	g_object_unref (priv->cdata);

	G_OBJECT_CLASS (li_ipk_control_parent_class)->finalize (object);
}

/**
 * li_ipk_control_init:
 **/
static void
li_ipk_control_init (LiIPKControl *ipkc)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	priv->cdata = li_config_data_new ();
}

/**
 * li_ipk_control_load_data:
 */
void
li_ipk_control_load_data (LiIPKControl *ipkc, const gchar *data)
{
	LiIPKControlPrivate *priv = GET_PRIVATE (ipkc);
	li_config_data_load_data (priv->cdata, data);
}

/**
 * li_ipk_control_class_init:
 **/
static void
li_ipk_control_class_init (LiIPKControlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_ipk_control_finalize;
}

/**
 * li_ipk_control_new:
 *
 * Creates a new #LiIPKControl.
 *
 * Returns: (transfer full): a #LiIPKControl
 *
 **/
LiIPKControl *
li_ipk_control_new (void)
{
	LiIPKControl *ipkc;
	ipkc = g_object_new (LI_TYPE_IPK_CONTROL, NULL);
	return LI_IPK_CONTROL (ipkc);
}
