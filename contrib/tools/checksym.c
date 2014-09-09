/* Returns 0 if the named symbol is present, 1 if not
 *
 * (C) 2005 Mike Hearn <mike@plan99.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "checksym: needs two arguments, library and symbol name\n");
        return 1;
    }

    char *libname = argv[1];
    char *symname = argv[2];

    void *handle = dlopen(libname, RTLD_LAZY);
    if (dlsym(handle, symname)) return 0;
    return 1;
}
