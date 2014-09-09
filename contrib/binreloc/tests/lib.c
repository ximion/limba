#include "lib-binreloc.h"

char *
locate_libshared ()
{
	br_init_lib ((BrInitError *) 0);
	return br_find_exe ((char *) 0);
}
