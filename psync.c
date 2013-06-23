/*
 * iob -- IO Benchmarking Tool
 *
 * Copyright (C) 2013 Prasad Joshi <prasadjoshi.linux@gmail.com>
 *
 * The license below covers all files distributed with fio unless otherwise
 * noted in the file itself.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

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
