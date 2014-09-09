/* Dump the contents of the .metadata section, one string per line
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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>

#include <link.h>

/* support building for 32 bit and 64 bit platforms */
#ifndef DEM_WORDSIZE
# ifndef __x86_64
#  define DEM_WORDSIZE 32
# else
#  define DEM_WORDSIZE 64
# endif
#endif

#undef ElfW
#define ElfW(type) _ElfW (Elf, DEM_WORDSIZE, type)

#define elfcast(a, b) (ElfW(a)*)(b)

static void __attribute__((format(printf, 1, 2))) bail(char *message, ...)
{
    fprintf( stderr, "dumpverdefs: " );

    va_list args;
    va_start( args, message );
    vfprintf( stderr, message, args );
    va_end( args );

    exit( 1 );
}

static void help()
{
    printf( "dump-elf-metadata: a tool to dump the .metadata section of an ELF binary\n" \
            "Usage: dump-elf-metadata /path/to/binary\n\n" \
            "(C) 2005 Mike Hearn <mike@plan99.net>\n\n" );

    exit( 0 );
}

/* retrieves the string from the given strtab  */
static char *resolve_string(ElfW(Ehdr) *header, int tabindex, ElfW(Word) offset)
{
    ElfW(Shdr) *section = elfcast( Shdr, (char*)header + header->e_shoff );

    section = elfcast( Shdr, (char*)section + (tabindex * sizeof(ElfW(Shdr))) );

    assert( section->sh_type == SHT_STRTAB );

    char *string = (char*)header + section->sh_offset;

    return string + offset;
}

int main(int argc, char **argv)
{
    argc--; argv++; /* don't care about our own name  */

    if (!argc) help();

    char *name = argv[0];

    argc--; argv++;

    struct stat buf;
    int e = stat( name, &buf );
    if (e == -1) bail( "could not open %s: %s\n", name, strerror(errno) );

    int fd = open( name, O_RDONLY );
    if (fd < 0) bail( "could not open %s: %s\n", name, strerror(errno) );

    void *binary = mmap( NULL, buf.st_size, PROT_READ, MAP_SHARED, fd, 0 );
    if (binary == MAP_FAILED) bail( "could not mmap %s: %s\n", name, strerror(errno) );

    close( fd );

    ElfW(Ehdr) *header = binary;
    if (strncmp( (char *) &header->e_ident, ELFMAG, SELFMAG ) != 0)
        bail( "bad ident sequence, %s not an ELF file?\n", name );

    if (((header->e_ident[EI_CLASS] == ELFCLASS32) && (DEM_WORDSIZE != 32)) ||
        ((header->e_ident[EI_CLASS] == ELFCLASS64) && (DEM_WORDSIZE != 64)))
        bail( "32/64 mismatch: elfclass is %d but compiled for %dbit machine\n",
	      header->e_ident[EI_CLASS], DEM_WORDSIZE );

    ElfW(Shdr) *sectab  = binary + header->e_shoff;
    ElfW(Shdr) *section = NULL;
    int found = 0;
    for (section = sectab; section < &sectab[header->e_shnum]; section++)
    {
        if (!strcmp( resolve_string( header, header->e_shstrndx, section->sh_name ), ".metadata" ))
	{
	    found = 1;
	    break;
	}
    }

    if (!found)
	return 1;

    char *str = binary + section->sh_offset;
    while (*str)
    {
	printf("%s\n", str);
	str += strlen(str) + 1;
    }

    return 0;
}
