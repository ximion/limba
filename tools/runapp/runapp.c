
#define _GNU_SOURCE /* Required for CLONE_NEWNS */
#include <limba.h>
#include <li-config-data.h>

#include <sys/mount.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <gio/gio.h>

/**
 * create_mount_namespace:
 */
static int
create_mount_namespace (void)
{
	int mount_count;
	int res;

	g_debug ("creating new namespace");

	res = unshare (CLONE_NEWNS);
	if (res != 0) {
		g_print ("Failed to create new namespace: %s\n", strerror(errno));
		return 1;
	}

	g_debug ("mount bundle (private)");
	mount_count = 0;
	res = mount (APP_ROOT_PREFIX, APP_ROOT_PREFIX,
				 NULL, MS_PRIVATE, NULL);
	if (res != 0 && errno == EINVAL) {
		/* Maybe if failed because there is no mount
		 * to be made private at that point, lets
		 * add a bind mount there. */
		g_debug (("mount bundle (bind)\n"));
		res = mount (APP_ROOT_PREFIX, APP_ROOT_PREFIX,
					 NULL, MS_BIND, NULL);
		/* And try again */
		if (res == 0) {
			mount_count++; /* Bind mount succeeded */
			g_debug ("mount bundle (private)");
			res = mount (APP_ROOT_PREFIX, APP_ROOT_PREFIX,
						 NULL, MS_PRIVATE, NULL);
		}
	}

	if (res != 0) {
		g_error ("Failed to make prefix namespace private");
		goto error_out;
	}

	return 0;

error_out:
	while (mount_count-- > 0)
		umount (APP_ROOT_PREFIX);
	return 1;
}

/**
 * mount_overlay:
 * @bundle: An application bundle identifier
 */
static int
mount_overlay (const gchar *bundle)
{
	int res = 0;
	gchar *main_data_path = NULL;
	gchar *fname = NULL;
	GFile *file;
	const gchar *framework_uuid;
	gchar *tmp;
	LiPkgInfo *pki = NULL;

	/* check if the bundle exists */
	main_data_path = g_build_filename (LI_INSTALL_ROOT, bundle, "data", NULL);
	fname = g_build_filename (LI_INSTALL_ROOT, bundle, "control", NULL);
	file = g_file_new_for_path (fname);
	g_free (fname);

	if (!g_file_query_exists (file, NULL)) {
		fprintf (stderr, "The bundle '%s' does not exist.\n", bundle);
		res = 1;
		g_object_unref (file);
		goto out;
	}

	pki = li_pkg_info_new ();
	li_pkg_info_load_file (pki, file);
	g_object_unref (file);

	framework_uuid = li_pkg_info_get_framework_dependency (pki);
	if (framework_uuid == NULL) {
		g_error ("Sorry, I can not construct a new framework for this application to run in. Please do that manually!");
		res = 3;
		goto out;
	}

	if (g_strcmp0 (framework_uuid, "None") != 0) {
		/* mount the desired framework */
		gchar *bundle_path;

		bundle_path = g_build_filename (LI_INSTALL_ROOT, "tmp", framework_uuid, "data", NULL);
		if (!g_file_test (bundle_path, G_FILE_TEST_IS_DIR)) {
			fprintf (stderr, "The framework '%s' does not exist.\n", framework_uuid);
			res = 1;
			g_free (bundle_path);
			goto out;
		}

		tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s", APP_ROOT_PREFIX, bundle_path);
		res = mount ("", APP_ROOT_PREFIX,
					"overlayfs", MS_MGC_VAL | MS_RDONLY | MS_NOSUID, tmp);
		g_free (tmp);
		g_free (bundle_path);
		if (res != 0) {
			fprintf (stderr, "Unable to mount dependency directory.\n");
			res = 1;
			goto out;
		}
	}
	tmp = g_strdup_printf ("lowerdir=%s,upperdir=%s", APP_ROOT_PREFIX, main_data_path);
	res = mount ("", APP_ROOT_PREFIX,
				 "overlayfs", MS_MGC_VAL | MS_RDONLY | MS_NOSUID, tmp);
	g_free (tmp);
	if (res != 0) {
		fprintf (stderr, "Unable to mount directory.\n");
		res = 1;
		goto out;
	}

out:
	if (main_data_path != NULL)
		g_free (main_data_path);
	if (pki != NULL)
		g_object_unref (pki);

	return res;
}

/**
 * update_env_var_list:
 */
static void
update_env_var_list (const gchar *var, const gchar *item)
{
	const gchar *env;
	gchar *value;

	env = getenv (var);
	if (env == NULL || *env == 0) {
		setenv (var, item, 1);
	} else {
		value = g_strconcat (item, ":", env, NULL);
		setenv (var, value, 1);
		free (value);
	}
}

/**
 * main:
 */
int
main (gint argc, gchar *argv[])
{
	int ret;
	gchar *ar[3];
	gchar *bundle = NULL;
	gchar *executable = NULL;
	gchar **strv;
	uid_t uid=getuid(), euid=geteuid();

	if (uid>0 && uid==euid) {
		g_error ("This program needs the suid bit to be set to function correpkiy.");
		return 3;
	}

	if (argc <= 1) {
		fprintf (stderr, "No application-id was specified.\n");
		return 1;
	}

	strv = g_strsplit (argv[1], ":", 2);
	if (g_strv_length (strv) != 2) {
		g_strfreev (strv);
		fprintf (stderr, "No valid application bundle-executable found.\n");
		ret = 1;
		goto out;
	}

	bundle = g_strdup (strv[0]);
	executable = g_build_filename (APP_ROOT_PREFIX, strv[1], NULL);

	ret = create_mount_namespace ();
	if (ret > 0)
		goto out;

	ret = mount_overlay (bundle);
	if (ret > 0)
		goto out;

	/* Now we have everything we need CAP_SYS_ADMIN for, so drop setuid */
	setuid (getuid ());

	update_env_var_list ("LD_LIBRARY_PATH", APP_ROOT_PREFIX "/lib");
	update_env_var_list ("LD_LIBRARY_PATH", APP_ROOT_PREFIX "/usr/lib");

	ar[0] = argv[2];
	ar[1] = argv[2];
	ar[2] = NULL;

	ret = execv (executable, ar);

out:
	if (bundle)
		g_free (bundle);
	if (executable)
		g_free (executable);
	return ret;
}
