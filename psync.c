#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "ioengine.h"

static int read_block(int fd, void *buf, unsigned long block,
		unsigned long block_size)
{
	char		*b;
	off64_t		offset;
	unsigned long	remaining;
	ssize_t		rc;

	b		= buf;
	offset		= block * block_size;
	remaining	= block_size;

	while (remaining > 0) {
		rc = pread64(fd, b, remaining, offset);
		if (rc < 0) {
			if (errno == EINTR)
				continue;

			fprintf(stderr, "write failed: %s\n", strerror(errno));
			return -1;
		}

		b		+= rc;
		offset		+= rc;
		remaining	-= rc;
	}
	return 0;
}

static int write_block(int fd, void *buf, unsigned long block,
		unsigned long block_size)
{
	char		*b;
	off64_t		offset;
	unsigned long	remaining;
	ssize_t		rc;

	b		= buf;
	offset		= block * block_size;
	remaining	= block_size;

	while (remaining > 0) {
		rc = pwrite64(fd, b, remaining, offset);
		if (rc < 0) {
			if (errno == EINTR)
				continue;

			fprintf(stderr, "write failed: %s\n", strerror(errno));
			return -1;
		}

		b		+= rc;
		offset		+= rc;
		remaining	-= rc;
	}
	return 0;
}

struct ioengine psync_engine  = {
	.name		= "psync",
	.read_block	= read_block,
	.write_block	= write_block,
};
