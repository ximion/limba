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
 * @LI_KEYRING_KIND_ALL:		Peek all keyrings
 * @LI_KEYRING_KIND_USER:		Use the manual keyring, containing explicitly trusted keys
 * @LI_KEYRING_KIND_AUTOMATIC:	Use the automatic keyring, containing automatically added keys
 *
 * The error type.
 **/
typedef enum {
	LI_KEYRING_KIND_ALL,
	LI_KEYRING_KIND_USER,
	LI_KEYRING_KIND_AUTOMATIC,
	/*< private >*/
	LI_KEYRING_LAST
} LiKeyringKind;

/**
 * LiKeyringError:
 * @LI_KEYRING_ERROR_FAILED:		Generic failure
 * @LI_KEYRING_ERROR_LOOKUP:		An error occured while looking up a key
 * @LI_KEYRING_ERROR_KEY_UNKNOWN: 	The key we are dealing with is unknown
 * @LI_KEYRING_ERROR_IMPORT:		Importing of a key failed
 *
 * The error type.
 **/
typedef enum {
	LI_KEYRING_ERROR_FAILED,
	LI_KEYRING_ERROR_LOOKUP,
	LI_KEYRING_ERROR_KEY_UNKNOWN,
	LI_KEYRING_ERROR_IMPORT,
	/*< private >*/
	LI_KEYRING_ERROR_LAST
} LiKeyringError;

#define	LI_KEYRING_ERROR li_keyring_error_quark ()
GQuark li_keyring_error_quark (void);

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

G_END_DECLS

#endif /* __LI_KEYRING_H */
