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

#if !defined (__LIMBA_H) && !defined (LI_COMPILATION)
#error "Only <limba.h> can be included directly."
#endif

#ifndef __LI_KEYRING_H
#define __LI_KEYRING_H

#include <glib-object.h>

#define LI_TYPE_KEYRING			(li_keyring_get_type())
#define LI_KEYRING(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), LI_TYPE_KEYRING, LiKeyring))
#define LI_KEYRING_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), LI_TYPE_KEYRING, LiKeyringClass))
#define LI_IS_KEYRING(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), LI_TYPE_KEYRING))
#define LI_IS_KEYRING_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), LI_TYPE_KEYRING))
#define LI_KEYRING_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), LI_TYPE_KEYRING, LiKeyringClass))

G_BEGIN_DECLS

/**
 * LiKeyringKind:
 * @LI_KEYRING_KIND_NONE:		No specific keyring should be used
 * @LI_KEYRING_KIND_USER:		Use the manual keyring, containing explicitly trusted keys
 * @LI_KEYRING_KIND_AUTOMATIC:	Use the automatic keyring, containing automatically added keys
 * @LI_KEYRING_KIND_ALL:		Peek all keyrings
 *
 * Type of the keyring to validate against.
 **/
typedef enum {
	LI_KEYRING_KIND_NONE,
	LI_KEYRING_KIND_USER,
	LI_KEYRING_KIND_AUTOMATIC,
	LI_KEYRING_KIND_ALL,
	/*< private >*/
	LI_KEYRING_LAST
} LiKeyringKind;

/**
 * LiKeyringError:
 * @LI_KEYRING_ERROR_FAILED:		Generic failure
 * @LI_KEYRING_ERROR_LOOKUP:		An error occured while looking up a key
 * @LI_KEYRING_ERROR_KEY_UNKNOWN: 	The key we are dealing with is unknown
 * @LI_KEYRING_ERROR_IMPORT:		Importing of a key failed
 * @LI_KEYRING_ERROR_VERIFY:		Verification failed
 *
 * The error type.
 **/
typedef enum {
	LI_KEYRING_ERROR_FAILED,
	LI_KEYRING_ERROR_LOOKUP,
	LI_KEYRING_ERROR_KEY_UNKNOWN,
	LI_KEYRING_ERROR_IMPORT,
	LI_KEYRING_ERROR_VERIFY,
	/*< private >*/
	LI_KEYRING_ERROR_LAST
} LiKeyringError;

#define	LI_KEYRING_ERROR li_keyring_error_quark ()
GQuark li_keyring_error_quark (void);

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

typedef struct _LiKeyring		LiKeyring;
typedef struct _LiKeyringClass	LiKeyringClass;

struct _LiKeyring
{
	GObject			parent;
};

struct _LiKeyringClass
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

GType			li_keyring_get_type	(void);
LiKeyring		*li_keyring_new		(void);

gboolean		li_keyring_import_key (LiKeyring *kr,
									const gchar *fpr,
									LiKeyringKind kind,
									GError **error);
gchar			*li_keyring_verify_clear_signature (LiKeyring *kr,
									const gchar *sigtext,
									gchar **out_fpr,
									GError **error);

LiTrustLevel	li_keyring_process_pkg_signature (LiKeyring *kr,
									const gchar *sigtext,
									gchar **out_data,
									gchar **out_fpr,
									GError **error);

G_END_DECLS

#endif /* __LI_KEYRING_H */
