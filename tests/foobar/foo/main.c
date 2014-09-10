#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "libfoo.h"

#define ENABLE_BINRELOC
#include "prefix.h"


int main ()
{
	const char *configfile;
	FILE *f;

	configfile = BR_SYSCONFDIR( "/foo-config" );

	printf ("foobar version " FOOBAR_VERSION "\n");

	libfoo ();
	printf ("\n");
	printf ("'Configuration' file %s:\n", configfile);
	f = fopen (configfile, "r");
	if (!f)
	{
		fprintf (stderr, "cannot open file!\n");
		printf ("\nPress ENTER to exit this program.\n");
		getchar ();
		return 1;
	}

	while (!feof (f))
	{
		char data[1024 * 8 + 1];
		size_t bytes_read;

		bytes_read = fread (&data, 1, sizeof (data) - 1, f);
		data[bytes_read] = 0;
		printf ("%s", data);
	}
	fclose (f);

	printf ("\nPress ENTER to exit this program.\n");
        getchar ();
	return 0;
}
