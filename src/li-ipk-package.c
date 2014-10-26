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
 * SECTION:li-ipk-package
 * @short_description: Representation of a complete Listaller package
 */

#include "config.h"
#include "li-ipk-package.h"

#include <glib/gi18n-lib.h>
#include <archive_entry.h>
#include <archive.h>

typedef struct _LiIPKPackagePrivate	LiIPKPackagePrivate;
struct _LiIPKPackagePrivate
{
	gchar *filename;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiIPKPackage, li_ipk_package, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_ipk_package_get_instance_private (o))

/**
 * li_ipk_package_finalize:
 **/
static void
li_ipk_package_finalize (GObject *object)
{
	/* LiIPKPackage *ipk = LI_IPK_PACKAGE (object);
	LiIPKPackagePrivate *priv = GET_PRIVATE (ipk); */

	G_OBJECT_CLASS (li_ipk_package_parent_class)->finalize (object);
}

/**
 * li_ipk_package_init:
 **/
static void
li_ipk_package_init (LiIPKPackage *ipk)
{

}

static struct archive*
li_ipk_package_open_base_ipk (LiIPKPackage *ipk, const gchar *filename, GError **error)
{
	struct archive *ar;
	int res;

	/* create new archive object for reading */
	ar = archive_read_new ();
	/* disable compression, as the main tarball is not compressed */
	archive_read_support_filter_none (ar);
	/* ipk bundles are GNU Tarballs */
	archive_read_support_format_tar (ar);

	/* open the file, exit on error */
	res = archive_read_open_filename (ar, filename, (gsize) 4096);
	if (res != ARCHIVE_OK) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_ARCHIVE,
				_("Could not open IPK file! Error: %s"), archive_error_string (ar));
		archive_read_free (ar);
		return NULL;
	}

	return ar;
}

/**
 * li_ipk_package_open_file:
 */
gboolean
li_ipk_package_open_file (LiIPKPackage *ipk, const gchar *filename, GError **error)
{
	struct archive *ar;

	if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		g_set_error (error,
				LI_PACKAGE_ERROR,
				LI_PACKAGE_ERROR_NOT_FOUND,
				_("Package file '%s' was not found."), filename);
		return FALSE;
	}

	ar = li_ipk_package_open_base_ipk (ipk, filename, error);
	if (ar == NULL)
		return FALSE;

	return TRUE;
}

/**
 * li_package_error_quark:
 *
 * Return value: An error quark.
 **/
GQuark
li_package_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("LiPackageError");
	return quark;
}

/**
 * li_ipk_package_class_init:
 **/
static void
li_ipk_package_class_init (LiIPKPackageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_ipk_package_finalize;
}

/**
 * li_ipk_package_new:
 *
 * Creates a new #LiIPKPackage.
 *
 * Returns: (transfer full): a #LiIPKPackage
 *
 **/
LiIPKPackage *
li_ipk_package_new (void)
{
	LiIPKPackage *ipk;
	ipk = g_object_new (LI_TYPE_IPK_PACKAGE, NULL);
	return LI_IPK_PACKAGE (ipk);
}
