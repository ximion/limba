/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014-2016 Matthias Klumpp <matthias@tenstral.net>
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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_PACKAGE_H
#define __LI_PACKAGE_H

#include <glib-object.h>
#include "li-pkg-info.h"
#include "li-pkg-cache.h"

G_BEGIN_DECLS

#define LI_TYPE_PACKAGE (li_package_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiPackage, li_package, LI, PACKAGE, GObject)

struct _LiPackageClass
{
	GObjectClass		parent_class;
	/*< private >*/
	void (*_as_reserved1)	(void);
	void (*_as_reserved2)	(void);
	void (*_as_reserved3)	(void);
	void (*_as_reserved4)	(void);
	void (*_as_reserved5)	(void);
	void (*_as_reserved6)	(void);
	void (*_as_reserved7)	(void);
	void (*_as_reserved8)	(void);
};

/**
 * LI_IPK_MAGIC:
 *
 * Magic number for IPK packages.
 */
#define LI_IPK_MAGIC "limba1\t\034"

/**
 * LiPackageError:
 * @LI_PACKAGE_ERROR_FAILED:				Generic failure
 * @LI_PACKAGE_ERROR_NOT_FOUND:				A required file or entity was not found
 * @LI_PACKAGE_ERROR_ARCHIVE:				Error in the archive structure
 * @LI_PACKAGE_ERROR_DATA_MISSING:			Some data is missing in the archive
 * @LI_PACKAGE_ERROR_OVERRIDE:				Could not override file
 * @LI_PACKAGE_ERROR_EXTRACT:				Could not extract data
 * @LI_PACKAGE_ERROR_CHECKSUM_MISMATCH:		A checksum did not match
 * @LI_PACKAGE_ERROR_WRONG_ARCHITECTURE:	The package was built for a different architecture
 * @LI_PACKAGE_ERROR_SIGNATURE_BROKEN:		The signature of this package is broken
 * @LI_PACKAGE_ERROR_DOWNLOAD_NEEDED:		Package needs to be downloaded first before we can perfom this operation.
 *
 * The error type.
 **/
typedef enum {
	LI_PACKAGE_ERROR_FAILED,
	LI_PACKAGE_ERROR_NOT_FOUND,
	LI_PACKAGE_ERROR_ARCHIVE,
	LI_PACKAGE_ERROR_DATA_MISSING,
	LI_PACKAGE_ERROR_OVERRIDE,
	LI_PACKAGE_ERROR_EXTRACT,
	LI_PACKAGE_ERROR_CHECKSUM_MISMATCH,
	LI_PACKAGE_ERROR_WRONG_ARCHITECTURE,
	LI_PACKAGE_ERROR_SIGNATURE_BROKEN,
	LI_PACKAGE_ERROR_DOWNLOAD_NEEDED,
	/*< private >*/
	LI_PACKAGE_ERROR_LAST
} LiPackageError;

#define	LI_PACKAGE_ERROR li_package_error_quark ()
GQuark li_package_error_quark (void);

/**
 * LiTrustLevel:
 * @LI_TRUST_LEVEL_NONE:	We don't trust that software at all (usually means no signature was found)
 * @LI_TRUST_LEVEL_INVALID:	The package could not be validated, its signature might be broken.
 * @LI_TRUST_LEVEL_LOW:		Low trust level (signed and validated, but no trusted author)
 * @LI_TRUST_LEVEL_MEDIUM:	Medium trust level (we already have software by this author installed and auto-trust him)
 * @LI_TRUST_LEVEL_HIGH:	High trust level (The software author is in our trusted database)
 *
 * A simple indicator on how much we trust a software package.
 **/
typedef enum {
	LI_TRUST_LEVEL_NONE,
	LI_TRUST_LEVEL_INVALID,
	LI_TRUST_LEVEL_LOW,
	LI_TRUST_LEVEL_MEDIUM,
	LI_TRUST_LEVEL_HIGH,
	/*< private >*/
	LI_TRUST_LEVEL_LAST
} LiTrustLevel;

const gchar	*li_trust_level_to_text (LiTrustLevel level);

/**
 * LiPackageStage:
 * @LI_PACKAGE_STAGE_UNKNOWN:		Unknown stage
 * @LI_PACKAGE_STAGE_DOWNLOADING:	Package is being downloaded
 * @LI_PACKAGE_STAGE_VERIFYING:		A signature is being verified
 * @LI_PACKAGE_STAGE_INSTALLING:	Package is being installed
 * @LI_PACKAGE_STAGE_FINISHED:		All tasks have finished
 *
 * Stages emitted when performing actions on a #LiPackage which consist of several
 * smaller steps, like installing the package.
 **/
typedef enum {
	LI_PACKAGE_STAGE_UNKNOWN,
	LI_PACKAGE_STAGE_DOWNLOADING,
	LI_PACKAGE_STAGE_VERIFYING,
	LI_PACKAGE_STAGE_INSTALLING,
	LI_PACKAGE_STAGE_FINISHED,
	/*< private >*/
	LI_PACKAGE_STAGE_LAST
} LiPackageStage;

const gchar	*li_package_stage_to_string (LiPackageStage stage);


LiPackage		*li_package_new (void);

gboolean		li_package_open_file (LiPackage *pkg,
						const gchar *filename,
						GError **error);
gboolean		li_package_open_remote (LiPackage *pkg,
						LiPkgCache *cache,
						const gchar *pkid,
						GError **error);

gboolean		li_package_is_remote (LiPackage *pkg);
gboolean		li_package_download (LiPackage *pkg,
						GError **error);

gboolean		li_package_install (LiPackage *pkg,
						GError **error);

gboolean		li_package_get_auto_verify (LiPackage *pkg);
void			li_package_set_auto_verify (LiPackage *pkg,
							gboolean verify);

LiTrustLevel		li_package_verify_signature (LiPackage *pkg,
							GError **error);

const gchar		*li_package_get_install_root (LiPackage *pkg);
void			li_package_set_install_root (LiPackage *pkg,
							const gchar *dir);

const gchar		*li_package_get_id (LiPackage *pkg);
void			li_package_set_id (LiPackage *pkg,
						const gchar *unique_name);

LiPkgInfo		*li_package_get_info (LiPackage *pkg);

gboolean		li_package_has_embedded_packages (LiPackage *pkg);
GPtrArray		*li_package_get_embedded_packages (LiPackage *pkg);
LiPackage*		li_package_extract_embedded_package (LiPackage *pkg,
								LiPkgInfo *pki,
								GError **error);
gchar			*li_package_get_appstream_data (LiPackage *pkg);

void			li_package_extract_contents (LiPackage *pkg,
							const gchar *dest_dir,
							GError **error);

void			li_package_extract_appstream_icons (LiPackage *pkg,
								const gchar *dest_dir,
								GError **error);

G_END_DECLS

#endif /* __LI_PACKAGE_H */
