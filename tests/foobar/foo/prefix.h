/*
 * BinReloc - a library for creating relocatable executables
 * Written by: Mike Hearn <mike@theoretic.com>
 *             Hongli Lai <h.lai@chello.nl>
 * http://listaller.tenstral.net/
 *
 * This source code is public domain. You can relicense this code
 * under whatever license you want.
 *
 * See README and USAGE in the original source distribution for
 * more information and how to use this.
 */

#ifndef _PREFIX_H_
#define _PREFIX_H_

#ifdef ENABLE_BINRELOC

/* Note to developers: you should generally only use macros.
   All functions should only be used internally by BinReloc. */


/* These are convience macros that replace the ones usually used
   in Autoconf/Automake projects */
#ifndef BR_NO_MACROS
	#undef SELFPATH
	#undef PREFIX
	#undef PREFIXDIR
	#undef BINDIR
	#undef SBINDIR
	#undef DATADIR
	#undef LIBDIR
	#undef LIBEXECDIR
	#undef ETCDIR
	#undef SYSCONFDIR
	#undef CONFDIR
	#undef LOCALEDIR

	#define SELFPATH (br_thread_local_store (br_locate ("")))
	#define PREFIX (br_thread_local_store (br_locate_prefix ("")))
	#define PREFIXDIR (br_thread_local_store (br_locate_prefix ("")))
	#define BINDIR (br_thread_local_store (br_prepend_prefix ("", "/bin")))
	#define SBINDIR (br_thread_local_store (br_prepend_prefix ("", "/sbin")))
	#define DATADIR (br_thread_local_store (br_prepend_prefix ("", "/share")))
	#define LIBDIR (br_thread_local_store (br_prepend_prefix ("", "/lib")))
	#define LIBEXECDIR (br_thread_local_store (br_prepend_prefix ("", "/libexec")))
	#define ETCDIR (br_thread_local_store (br_prepend_prefix ("", "/etc")))
	#define SYSCONFDIR (br_thread_local_store (br_prepend_prefix ("", "/etc")))
	#define CONFDIR (br_thread_local_store (br_prepend_prefix ("", "/etc")))
	#define LOCALEDIR (br_thread_local_store (br_prepend_prefix ("", "/share/locale")))
#endif /* BR_NO_MACROS */


char *br_locate (void *symbol);
char *br_locate_prefix (void *symbol);
char *br_prepend_prefix (void *symbol, char *path);

#endif /* ENABLE_BINRELOC */


/* These macros and functions are not guarded by the ENABLE_BINRELOC
 * macro because they are portable.
 */

#ifndef BR_NO_MACROS
	/* Convenience functions for concatenating paths */
	#define BR_SELFPATH(suffix) (br_thread_local_store (br_strcat (SELFPATH, suffix)))
	#define BR_PREFIX(suffix) (br_thread_local_store (br_strcat (PREFIX, suffix)))
	#define BR_PREFIXDIR(suffix) (br_thread_local_store (br_strcat (BR_PREFIX, suffix)))
	#define BR_BINDIR(suffix) (br_thread_local_store (br_strcat (BINDIR, suffix)))
	#define BR_SBINDIR(suffix) (br_thread_local_store (br_strcat (SBINDIR, suffix)))
	#define BR_DATADIR(suffix) (br_thread_local_store (br_strcat (DATADIR, suffix)))
	#define BR_LIBDIR(suffix) (br_thread_local_store (br_strcat (LIBDIR, suffix)))
	#define BR_ETCDIR(suffix) (br_thread_local_store (br_strcat (ETCDIR, suffix)))
	#define BR_SYSCONFDIR(suffix) (br_thread_local_store (br_strcat (SYSCONFDIR, suffix)))
	#define BR_CONFDIR(suffix) (br_thread_local_store (br_strcat (CONFDIR, suffix)))
	#define BR_LOCALEDIR(suffix) (br_thread_local_store (br_strcat (LOCALEDIR, suffix)))
#endif

const char *br_thread_local_store (char *str);
char *br_strcat (const char *str1, const char *str2);
char *br_extract_dir (const char *path);
char *br_extract_prefix (const char *path);

#endif /* _PREFIX_H_ */
