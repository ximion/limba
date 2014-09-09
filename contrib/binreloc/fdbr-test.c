/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int main()
{
    int fd = open("/proc/self/fd/200/binreloc/fdbr.c", O_RDONLY);
    int num = 0;
    char buf[100];

    memset(buf, 0, sizeof(buf));

    while ((num = read(fd, buf, sizeof(buf) - 2)) > 0)
    {
        buf[num] = '\0';
        printf("%s", buf);
    }

}
