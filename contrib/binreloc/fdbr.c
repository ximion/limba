/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

/* File descriptor based binary relocatability
 *
 * Summary: open a file descriptor pointing to the runtime determined
 * prefix directory. Dup2 this to a reserved FD number like 200 or
 * so. Then compile prefix to be /proc/self/fd/200. Checks that we're
 * not suid+linked.
 */

#define _GNU_SOURCE
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static char *get_self_path()
{
    char *exe = NULL;
    size_t size = 100;

    while (1)
    {
        exe = (char *) realloc(exe, size);
	/* Handle OOM (Tracker issue #35) */
	if (!exe)
		return NULL;
        memset(exe, 0, size);

        int nchars = readlink("/proc/self/exe", exe, size);

        if (nchars < 0)
        {
            free(exe);
            return NULL;
        }

        if (nchars < size) return exe;
        size *= 2;
    }
}

static char *dirname (const char *path)
{
    char *end, *result;

    if (!path) return NULL;

    end = strrchr (path, '/');
    if (end == (const char *) NULL)
        return strdup (".");

    while (end > path && *end == '/')
        end--;

    result = strndup (path, end - path + 1);

    if (result[0] == 0)
    {
        free (result);
        return strdup ("/");
    }

    return result;
}

static int is_secure()
{
    /* A suid relocatable program could be attacked via hard
     * links, forcing it to read a file that it did not expect.
     *
     * To prevent this we fall back to /usr and print a warning if
     * we are suid and link count is not 1.
     */

    if (getuid() == geteuid()) return 1;

    struct stat buf;

    if (stat("/proc/self/exe", &buf) < 0)
    {
        perror("stat");
        return 0;
    }

    if (buf.st_nlink > 1) return 0;

    return 1;
}



void __attribute__((visibility("hidden"))) __attribute__((constructor)) init_prefix_fd()
{
    char *prefix;

    /* suid/link check */
    if (!is_secure())
    {
        fprintf(stderr, "init_prefix_fd(): I am suid and hard linked, relocatability security check failed\n");
        fprintf(stderr, "init_prefix_fd(): Assuming installation prefix of /usr\n");
        prefix = strdup("/usr");
    }
    else
    {
        /* find the prefix path */
        char *exepath = get_self_path();
        char *exedir  = dirname(exepath);

        prefix = dirname(exedir);
        printf("exepath=%s, exedir=%s, prefix=%s\n", exepath, exedir, prefix);

        if (exepath) free(exepath);
        if (exedir)  free(exedir);

        if (!prefix) return;
    }

    int fd = -1;
    const int requested_fd = 200;

    /* open it  */
    if ((fd = open(prefix, O_RDONLY)) < 0)
    {
        perror("open");

        free(prefix);
        return;
    }

    if (dup2(fd, requested_fd) < 0)
    {
        perror("dup2");

        free(prefix);
        close(fd);

        return;
    }

    free(prefix);
    close(fd);

    /* And intentionally leak the fd so it can be used to look up files.
     *
     * Note that we deliberately allow it to survive exec(), because
     * the program may pass internal paths to other processes. If that
     * other process _also_ uses fd binreloc then dup2 will close the
     * previous fd so the path will no longer be valid. The numbers
     * can be changed to avoid that conflict, or alternatively the
     * path can be normalised using realpath() before passing it
     * across. Namespaces are process specific anyway so this obscure
     * gotcha is not limited to binreloc.
     */
}
