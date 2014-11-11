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
 * SECTION:li-exporter
 * @short_description: Export certain files from the software environment in /opt to achieve better system integration.
 */

#include "config.h"
#include "li-exporter.h"

#include <glib/gi18n-lib.h>
#include "li-utils-private.h"

typedef struct _LiExporterPrivate	LiExporterPrivate;
struct _LiExporterPrivate
{
	GPtrArray *external_files;
	gboolean override;
	gchar *pkgid;
};

G_DEFINE_TYPE_WITH_PRIVATE (LiExporter, li_exporter, G_TYPE_OBJECT)

#define GET_PRIVATE(o) (li_exporter_get_instance_private (o))

/**
 * li_exporter_finalize:
 **/
static void
li_exporter_finalize (GObject *object)
{
	LiExporter *exp = LI_EXPORTER (object);
	LiExporterPrivate *priv = GET_PRIVATE (exp);

	g_free (priv->pkgid);
	g_ptr_array_unref (priv->external_files);

	G_OBJECT_CLASS (li_exporter_parent_class)->finalize (object);
}

/**
 * li_exporter_init:
 **/
static void
li_exporter_init (LiExporter *exp)
{
	LiExporterPrivate *priv = GET_PRIVATE (exp);

	priv->override = FALSE;
	priv->external_files = g_ptr_array_new_with_free_func (g_free);
}

/**
 * li_copy_file:
 */
static void
li_exporter_copy_file (LiExporter *exp, const gchar *source, const gchar *destination, GError **error)
{
	GError *tmp_error = NULL;
	gchar *contents = NULL;
	LiExporterPrivate *priv = GET_PRIVATE (exp);

	if ((!priv->override) && (g_file_test (destination, G_FILE_TEST_EXISTS))) {
		g_set_error (error,
					G_FILE_ERROR,
					G_FILE_ERROR_EXIST,
					_("File '%s' already exists."), destination);
		return;
	}

	g_file_get_contents (source, &contents, NULL, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		return;
	}

	g_file_set_contents (destination, contents, -1, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
	}

	g_free (contents);
}

/**
 * li_exporter_process_desktop_file:
 */
static gboolean
li_exporter_process_desktop_file (LiExporter *exp, const gchar *disk_location, GError **error)
{
	_cleanup_free_ gchar *dest;
	gchar *tmp;
	GError *tmp_error = NULL;
	GKeyFile *kfile = NULL;
	_cleanup_free_ gchar *exec_cmd = NULL;
	LiExporterPrivate *priv = GET_PRIVATE (exp);

	tmp = g_path_get_basename (disk_location);
	dest = g_build_filename (LI_PREFIXDIR, "share", "applications", tmp, NULL);
	g_free (tmp);

	li_exporter_copy_file (exp, disk_location, dest, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	kfile = g_key_file_new ();
	g_key_file_load_from_file (kfile, dest,
							G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS,
							&tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

	exec_cmd = g_key_file_get_string (kfile, "Desktop Entry", "Exec", &tmp_error);
	if (tmp_error != NULL) {
		/* this is not fatal, ignore the error */
		g_error_free (tmp_error);
		goto out;
	}

	if (g_strrstr (exec_cmd, "%RUNAPP%") == NULL) {
		if (g_str_has_prefix (exec_cmd, "/")) {
			tmp = g_strdup_printf ("runapp %s:%s", priv->pkgid, exec_cmd);
		} else {
			/* just guess that the application is in bin */
			tmp = g_strdup_printf ("runapp %s:/bin/%s", priv->pkgid, exec_cmd);
		}
		g_free (exec_cmd);
		exec_cmd = tmp;
	} else {
		gchar *tmp1;
		gchar *tmp2;

		/* process the runapp placeholder */

		tmp1 = g_strdup_printf ("runapp %s:", priv->pkgid);
		tmp2 = li_str_replace (exec_cmd, "%RUNAPP%", tmp1);
		g_free (tmp1);

		g_free (exec_cmd);
		exec_cmd = tmp2;
	}
	g_key_file_set_string (kfile, "Desktop Entry", "Exec", exec_cmd);

	g_key_file_save_to_file (kfile, dest, &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, tmp_error);
		goto out;
	}

out:
	/* register installation of new external file */
	g_ptr_array_add (priv->external_files, g_strdup (dest));

	if (kfile != NULL)
		g_key_file_unref (kfile);

	return TRUE;
}

/**
 * li_exporter_process_file:
 */
gboolean
li_exporter_process_file (LiExporter *exp, const gchar *filename, const gchar *disk_location, GError **error)
{
	gboolean ret = FALSE;

	if (!g_file_test (disk_location, G_FILE_TEST_IS_REGULAR)) {
		/* ignore stuff which isn't a file */
		return FALSE;
	}

	if ((g_str_has_prefix (filename, "share/applications")) && (g_str_has_suffix (filename, ".desktop")))
		ret = li_exporter_process_desktop_file (exp, disk_location, error);

	return ret;
}

/**
 * li_exporter_get_exported_files_index:
 */
gchar*
li_exporter_get_exported_files_index (LiExporter *exp)
{
	guint i;
	GString *res;
	LiExporterPrivate *priv = GET_PRIVATE (exp);

	res = g_string_new ("");
	for (i = 0; i < priv->external_files->len; i++) {
		gchar *checksum;
		const gchar *fname = (const gchar *) g_ptr_array_index (priv->external_files, i);

		checksum = li_compute_checksum_for_file (fname);
		if (checksum == NULL)
			checksum = g_strdup ("ERROR");
		g_string_append_printf (res, "%s\t%s\n", checksum, fname);
		g_free (checksum);
	}

	return g_string_free (res, FALSE);
}

/**
 * li_exporter_get_pkgid:
 */
const gchar*
li_exporter_get_pkgid (LiExporter *exp)
{
	LiExporterPrivate *priv = GET_PRIVATE (exp);
	return priv->pkgid;
}

/**
 * li_exporter_set_pkgid:
 */
void
li_exporter_set_pkgid (LiExporter *exp, const gchar *pkgid)
{
	LiExporterPrivate *priv = GET_PRIVATE (exp);
	g_free (priv->pkgid);
	priv->pkgid = g_strdup (pkgid);
}

/**
 * li_ipk_package_get_install_root:
 */
gboolean
li_exporter_get_override_allowed (LiExporter *exp)
{
	LiExporterPrivate *priv = GET_PRIVATE (exp);
	return priv->override;
}

/**
 * li_exporter_set_override_allowed:
 *
 * Allow overriding of exported files
 */
void
li_exporter_set_override_allowed (LiExporter *exp, gboolean override)
{
	LiExporterPrivate *priv = GET_PRIVATE (exp);
	priv->override = override;
}

/**
 * li_exporter_class_init:
 **/
static void
li_exporter_class_init (LiExporterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = li_exporter_finalize;
}

/**
 * li_exporter_new:
 *
 * Creates a new #LiExporter.
 *
 * Returns: (transfer full): a #LiExporter
 *
 **/
LiExporter *
li_exporter_new (void)
{
	LiExporter *exp;
	exp = g_object_new (LI_TYPE_EXPORTER, NULL);
	return LI_EXPORTER (exp);
}
