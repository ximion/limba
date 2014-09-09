/* c-basic-offset: 4 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
	fprintf(stderr, "freespace: not enough arguments, argc=%d\n", argc);
	return 1;
    }

    struct statvfs buf;
    if (statvfs(argv[1], &buf) != 0) {
	fprintf(stderr, "freespace: statvfs failed: %d\n", errno);
	return 2;
    }

    unsigned long long bytesfree = (unsigned long long) buf.f_bsize * (unsigned long long) buf.f_bfree;
    if (argc >= 3) {
        /* Check whether there's enough free space */
        unsigned long long required = (unsigned long long) atoll(argv[2]);
        if (bytesfree >= required)
            return 0;
        else
            return 1;
    } else {
        /* Report free space */
        printf("%llu\n", bytesfree);
        return 0;
    }
}
