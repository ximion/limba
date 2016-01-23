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

#ifndef __LI_PKG_INFO_H
#define __LI_PKG_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define LI_TYPE_PKG_INFO (li_pkg_info_get_type ())
G_DECLARE_DERIVABLE_TYPE (LiPkgInfo, li_pkg_info, LI, PKG_INFO, GObject)

struct _LiPkgInfoClass
{
	GObjectClass	parent_class;
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
 * LiVersionFlags:
 * @LI_VERSION_UNKNOWN:	The relation is unknown
 * @LI_VERSION_EQUAL:	Versions should be equal
 * @LI_VERSION_LOWER:	The other version must be lower
 * @LI_VERSION_HIGHER:	The other version must be higher
 *
 * Flags defining version requirements on other #LiPkgInfo instances.
 **/
typedef enum  {
	LI_VERSION_UNKNOWN = 0,
	LI_VERSION_EQUAL = 1 << 0,
	LI_VERSION_LOWER = 1 << 1,
	LI_VERSION_HIGHER = 1 << 2
} LiVersionFlags;

/**
 * LiPackageFlags:
 * @LI_PACKAGE_FLAG_NONE:		No package flag is set
 * @LI_PACKAGE_FLAG_APPLICATION:	This package needs a runtime
 * @LI_PACKAGE_FLAG_AUTOMATIC:		This package has been installed automatically
 * @LI_PACKAGE_FLAG_FADED:		Remove this package automatically, if it is no longer in use
 * @LI_PACKAGE_FLAG_AVAILABLE:		Package is available in a repository
 * @LI_PACKAGE_FLAG_INSTALLED:		Package is installed
 *
 * Flags defining version requirements on other #LiPkgInfo instances.
 **/
typedef enum  {
	LI_PACKAGE_FLAG_NONE = 0,
	LI_PACKAGE_FLAG_APPLICATION = 1 << 0,
	LI_PACKAGE_FLAG_AUTOMATIC = 1 << 1,
	LI_PACKAGE_FLAG_FADED = 1 << 2,
	LI_PACKAGE_FLAG_AVAILABLE = 1 << 3,
	LI_PACKAGE_FLAG_INSTALLED = 1 << 4,
} LiPackageFlags;

/**
 * LiPackageKind:
 * @LI_PACKAGE_KIND_UNKNOWN:		Unknown package type
 * @LI_PACKAGE_KIND_COMMON:		Standard Limba package
 * @LI_PACKAGE_KIND_DEVEL:		Development package (SDK)
 *
 * Type of the Limba package
 **/
typedef enum  {
	LI_PACKAGE_KIND_UNKNOWN,
	LI_PACKAGE_KIND_COMMON,
	LI_PACKAGE_KIND_DEVEL,
} LiPackageKind;

const gchar	*li_package_kind_to_string (LiPackageKind kind);
LiPackageKind	li_package_kind_from_string (const gchar *kind_str);


LiPkgInfo	*li_pkg_info_new	(void);

void		li_pkg_info_load_file (LiPkgInfo *pki,
					GFile *file,
					GError **error);
void 		li_pkg_info_load_data (LiPkgInfo *pki,
					const gchar *data);
gboolean	li_pkg_info_save_to_file (LiPkgInfo *pki,
					const gchar *filename);
gboolean	li_pkg_info_save_changes (LiPkgInfo *pki);

const gchar	*li_pkg_info_get_version (LiPkgInfo *pki);
void		li_pkg_info_set_version (LiPkgInfo *pki,
					const gchar *version);

const gchar	*li_pkg_info_get_name (LiPkgInfo *pki);
void		li_pkg_info_set_name (LiPkgInfo *pki,
					const gchar *name);

const gchar	*li_pkg_info_get_appname (LiPkgInfo *pki);
void		li_pkg_info_set_appname (LiPkgInfo *pki,
					const gchar *app_name);

const gchar	*li_pkg_info_get_runtime_dependency (LiPkgInfo *pki);
void		li_pkg_info_set_runtime_dependency (LiPkgInfo *pki,
					const gchar *uuid);

const gchar	*li_pkg_info_get_dependencies (LiPkgInfo *pki);
void		li_pkg_info_set_dependencies (LiPkgInfo *pki,
					const gchar *deps_string);

const gchar	*li_pkg_info_get_sdk_dependencies (LiPkgInfo *pki);
void		li_pkg_info_set_sdk_dependencies (LiPkgInfo *pki,
					const gchar *deps_string);

const gchar	*li_pkg_info_get_build_dependencies (LiPkgInfo *pki);
void		li_pkg_info_set_build_dependencies (LiPkgInfo *pki,
					const gchar *deps_string);

const gchar	*li_pkg_info_get_id (LiPkgInfo *pki);
void		li_pkg_info_set_id (LiPkgInfo *pki,
					const gchar *id);

const gchar	*li_pkg_info_get_checksum_sha256 (LiPkgInfo *pki);
void		li_pkg_info_set_checksum_sha256 (LiPkgInfo *pki,
					const gchar *hash);

LiPackageKind	li_pkg_info_get_kind (LiPkgInfo *pki);
void		li_pkg_info_set_kind (LiPkgInfo *pki,
					LiPackageKind kind);

const gchar	*li_pkg_info_get_component_kind (LiPkgInfo *pki);
void		li_pkg_info_set_component_kind (LiPkgInfo *pki,
					const gchar *kind);

void		li_pkg_info_set_flags (LiPkgInfo *pki,
					LiPackageFlags flags);
void		li_pkg_info_add_flag (LiPkgInfo *pki,
					LiPackageFlags flag);
gboolean	li_pkg_info_has_flag (LiPkgInfo *pki,
					LiPackageFlags flag);
LiPackageFlags	li_pkg_info_get_flags (LiPkgInfo *pki);

void		li_pkg_info_set_version_relation (LiPkgInfo *pki,
					LiVersionFlags vrel);
LiVersionFlags	li_pkg_info_get_version_relation (LiPkgInfo *pki);

const gchar	*li_pkg_info_get_architecture (LiPkgInfo *pki);
void		li_pkg_info_set_architecture (LiPkgInfo *pki,
					const gchar *arch);
gboolean	li_pkg_info_matches_current_arch (LiPkgInfo *pki);

const gchar	*li_pkg_info_get_repo_location (LiPkgInfo *pki);
void		li_pkg_info_set_repo_location (LiPkgInfo *pki,
					const gchar *location);

gchar		*li_pkg_info_get_name_relation_string (LiPkgInfo *pki);

gboolean	li_pkg_info_satisfies_requirement (LiPkgInfo *pki,
						LiPkgInfo *req);

const gchar	*li_pkg_info_get_abi_break_versions (LiPkgInfo *pki);
void		li_pkg_info_set_abi_break_versions (LiPkgInfo *pki,
					const gchar *versions);

G_END_DECLS

#endif /* __LI_PKG_INFO_H */
