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

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <time.h>

#include <assert.h>

#include "random.h"
#include "ioengine.h"


#define IO_BLOCK_SIZE	4096
#define MAX_DEVICES	24
#define MAX_PROC_DEVICE	256
#define MAX_PROCESSES	2048

#define SEC_TO_MILLI	(10ULL * 10ULL * 10ULL)
#define SEC_TO_MICRO	((10ULL * 10ULL * 10ULL) * (SEC_TO_MILLI))
#define SEC_TO_NS	((10ULL * 10ULL * 10ULL) * (SEC_TO_MICRO))

extern struct ioengine sync_engine;
extern struct ioengine psync_engine;

struct ioengine *engines[] = {
	&sync_engine,
	&psync_engine,
};

char *buf;

struct result_data {
	unsigned long long	total_write_latency;
	unsigned long long	total_read_latency;

	unsigned long		writes;
	unsigned long		reads;

	/* input parameters for calculating result */
	int			device_index;
};

struct thread_data {
	unsigned long		start_block;
	unsigned long		end_block;
	unsigned long		block_size;
	unsigned int		iterations;	/* # iterations */
	int			verify;		/* verify wrote data */
	int			random;		/* random IOs */
	int			clk_id;		/* clock ID */
	struct ioengine		*ioengine;	/* selected io engine */

	int			fd;

	struct result_data	*result;
};

static const key_t	shm_key	 = 1234;
static const int	shm_size = sizeof(struct result_data) * MAX_PROCESSES;
int			shm_id;
char			*shm = NULL;
char			**devices;

void usage(char *str)
{
	char buf[1024];

	fprintf(stderr, "Usage: \n");
	fprintf(stderr, "\t%s [OPTIONS] [PATHS]\n\n", str);
	
	fprintf(stderr, "Summary of OPTIONS: \n");
	
	fprintf(stderr, "\t%-20s\t%s\n", "-p <pattern>", "IO data pattern");
	
	fprintf(stderr, "\t%-20s\t%s\n", "-n <# processes>",
			"Number of threads (for each path) for parallel IO.");
	
	fprintf(stderr, "\t%-20s\t%s\n", "-i <# iterations>",
			"Interations for IO. (1)");

	fprintf(stderr, "\t%-20s\t%s\n", "-s <# seconds>",
			"Number of seconds to run.");
	
	sprintf(buf, "Block size for IO. (%d)", IO_BLOCK_SIZE);
	fprintf(stderr, "\t%-20s\t%s\n", "-b <block size>", buf);

	fprintf(stderr, "\t%-20s\t%s\n", "-R", "Random IOs");

	fprintf(stderr, "\t%-20s\t%s\n", "-E <engine>",
			"IO engine to use. Either sync or psync. (psync).");

	fprintf(stderr, "\t%-20s\t%s\n", "-S <size>",
			"Device size in GB. (10)");

	fprintf(stderr, "\n");
}

#if !defined(mempcpy)
void *mempcpy(void *dest, const void *src, size_t n)
{
	bcopy(src, dest, n);
	return dest + n;
}
#endif

void fill_buf(char *buf, unsigned long buf_size, char *pattern)
{
	int l = strlen(pattern);
	char *p;
	int i;

	p = buf;
	while (((p - buf) + l) < buf_size) {
		p = mempcpy(p, pattern, l);
	}

	i = 0;
	while ((p - buf) < buf_size) {
		*p = pattern[i++];
		p++;
	}
}

struct ioengine *get_ioengine(const char *name)
{
	int i;
	int len = IOENGIN_NAME_LENGTH;

	for (i = 0; sizeof(engines) / sizeof(engines[0]); i++) {
		if (!strncmp(engines[i]->name, name, len))
			return engines[i];
	}
	return NULL;
}

static int get_clock_id(void)
{
	int i;
	int clk_ids[] = {
#if defined(CLOCK_PROCESS_CPUTIME_ID)
		CLOCK_PROCESS_CPUTIME_ID,
#endif

#if defined(CLOCK_REALTIME)
		CLOCK_REALTIME,
#endif

#if defined(CLOCK_MONOTONIC)
		CLOCK_MONOTONIC,
#endif

#if defined(CLOCK_MONOTONIC_RAW)
		CLOCK_MONOTONIC_RAW,
#endif

#if defined(CLOCK_THREAD_CPUTIME_ID)
		CLOCK_THREAD_CPUTIME_ID,
#endif

#if defined(CLOCK_MONOTONIC_COARSE)
		CLOCK_MONOTONIC_COARSE,
#endif
	};

	struct timespec	res;
	long		min_nsec = 1000;
	int		cid      = -1;

	for (i = 0; i < sizeof(clk_ids) / sizeof(clk_ids[0]); i++) {
		if (clock_getres(clk_ids[i], &res) < 0)
			continue;

		if (min_nsec > res.tv_nsec) {
			min_nsec = res.tv_nsec;
			cid      = clk_ids[i];
		}
	}

	return cid;
}

unsigned long long get_hrtime(int clk_id)
{
	struct timespec ts;

	if (clock_gettime(clk_id, &ts) < 0)
		return 0;
	return ts.tv_sec * SEC_TO_NS + ts.tv_nsec;
}

int do_io(struct thread_data *td)
{
	unsigned long		sb;
	unsigned long		eb;
	unsigned long		iterations;
	int			fd;
	struct result_data	*rd;
	int			use_iteration;
	int			clk_id;
	struct ioengine		*ioengine;

	unsigned long		blocks;
	int			r_b_size;
	unsigned long		*r_b;
	struct rand_range	ir;
	int			i;
	unsigned long		block_size = td->block_size;

	unsigned long		b;
	char			lb[block_size + 1];

	unsigned long long s;
	unsigned long long e;

	sb		= td->start_block;
	eb		= td->end_block;
	iterations	= td->iterations;
	fd		= td->fd;
	rd		= td->result;
	use_iteration	= (iterations == 0) ? 0 : 1;
	clk_id		= td->clk_id;
	ioengine	= td->ioengine;

	blocks		= eb - sb + 1;
	r_b_size	= sizeof(*r_b) * blocks;
	r_b		= malloc(r_b_size);
	memset(r_b, 0, r_b_size);

	init_rand_range(&ir, sb, eb);
	for (i = 0, b = sb ; i < blocks; i++, b++) {
		if (!td->random) {
			r_b[i] = b;
			continue;
		}

		r_b[i] = get_random_range(&ir);
	}

	rd->total_write_latency = 0;
	rd->writes = 0;
	rd->total_read_latency = 0;
	rd->reads = 0;
	while (!use_iteration || iterations) {

		s  = get_hrtime(clk_id);
		/* write blocks */
		for (i = 0; i < blocks; i++) {
			if (ioengine->write_block(fd, buf, r_b[i], block_size) < 0) {
				e  = get_hrtime(clk_id);
				rd->total_write_latency += (e - s);
				rd->writes += i;
				return -1;
			}
		}
		e  = get_hrtime(clk_id);
		rd->total_write_latency += (e - s);
		rd->writes += blocks;

		if (iterations > 0)
			iterations--;

		if (!td->verify)
			continue;

		/* verify the written data */
		for (b = sb; b <= eb; b++) {
			if (ioengine->read_block(fd, lb, b, block_size) < 0) {
				return -1;
			}

			if (memcmp(lb, buf, block_size)) {
				printf("Possible Data Corruption\n");
			}
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char		*program;	/* program name */
	char		*pattern;	/* pattern for IO */
	int		procs;		/* number of processes */
	unsigned long	seconds;	/* number of seconds to run */
	unsigned long	iterations;	/* number of iterations */
	int		verify;
	int		random;		/* radom writes and reads */
	int		clk_id;		/* clock ID for high resolution timer */
	char		*s_engine;	/* IO Engine name */
	int		opt;
	int		no_devices;

	int			dev_size_gb;	/* device sizre in GB */
	unsigned long long	dev_size;	/* device size in bytes */
	unsigned long		block_size;	/* device block size */
	unsigned long		blocks;		/* # total blocks */
	unsigned long		p_blocks;	/* # blocks for each proccess */

	pid_t 		pids[MAX_PROCESSES];
	pid_t		pid;
	unsigned long	start_block;
	unsigned long	end_block;
	int		i;
	int		d;

	struct result_data	*rd;
	struct ioengine		*ioengine;

	unsigned long		total_reads, total_writes;
	unsigned long long	avg_write_latency, avg_read_latency;
	unsigned long long	write_latency, read_latency;
	unsigned long long	read_bw, write_bw;

	program		= argv[0];
	pattern		= NULL;
	procs		= 1;	/* default only one process */
	seconds		= 0;
	iterations	= 1;	/* default: only one iteration */
	verify		= 0;	/* default: do not verify wrote data */
	block_size	= IO_BLOCK_SIZE; /* default: block size is 4096 */
	random		= 0;
	s_engine	= NULL;
	ioengine	= &psync_engine; /* default: psync io engine */
	dev_size_gb	= 10;		/* default: 10 GB */

	while ((opt = getopt(argc, argv, "p:n:s:i:Vb:RE:S:")) != -1) {
		switch (opt) {
		case 'p': /* pattern */
			pattern = strdup(optarg);
			break;
		case 'n': /* number of processes */
			procs = atoi(optarg);
			break;
		case 's': /* seconds to run */
			seconds = atol(optarg);
			break;
		case 'i': /* number of iterations */
			iterations = atol(optarg);
			break;
		case 'V': /* data verify */
			verify = 1;
			break;
		case 'b': /* block size */
			block_size = atol(optarg);
			break;
		case 'R': /* random */
			random = 1;
			break;
		case 'E': /* IO Engine */
			s_engine = strdup(optarg);
			break;
		case 'S': /* device size in GB */
			dev_size_gb = atoi(optarg);
			break;
		}
	}

	if (optind == argc) {
		usage(program);
		return 1;
	}

	clk_id = get_clock_id();
	if (clk_id < 0) {
		fprintf(stderr, "Finding high resolution clock failed.\n");
		return 1;
	}

	no_devices = 0;
	for (i = optind; argv[i]; i++) {
		devices = realloc(devices, sizeof(devices) * (no_devices + 1));
		if (!devices) {
			fprintf(stderr, "Memory Allocation Failed.\n");
			return 1;
		}

		devices[no_devices] = strdup(argv[i]);
		no_devices++;
	}


	if (!procs || (!seconds && !iterations)) {
		usage(program);
		return 1;
	}

	if (procs > MAX_PROC_DEVICE) {
		fprintf(stderr, "Number of processes should be less than %d\n",
				MAX_PROCESSES);
		usage(program);
		return 1;
	}

	if (s_engine) {
		ioengine = get_ioengine(s_engine);
		if (!ioengine) {
			usage(program);
			return 1;
		}
	}

	if (no_devices * procs > MAX_PROCESSES) {
		fprintf(stderr, "Will create only %d processes.\n", MAX_PROCESSES);
		procs = MAX_PROCESSES / no_devices;
	}

	if (!pattern) {
		/* use default pattern */
		pattern = "[Hello, World!]";
	}

	/* verify device paths */
	for (i = 0; i < no_devices; i++) {
		struct stat buf;

		if (stat(devices[i], &buf) < 0) {
			fprintf(stderr, "stat(%s) failed: %s\n", devices[i], strerror(errno));
			return 1;
		}

		switch (buf.st_mode & S_IFMT) {
		case S_IFBLK:
		case S_IFCHR:
		case S_IFREG:
			break;
		default:
			fprintf(stderr, "%s is not a valid block device.\n", devices[i]);
			usage(program);
			return 1;	
		}
	}

	if (seconds)
		iterations = 0;

	/* initialize shared memory for results */
	if ((shm_id = shmget(shm_key, shm_size, IPC_CREAT | 0666)) < 0) {
		fprintf(stderr, "shmget failed: %s.\n", strerror(errno));
		return 1;
	}

	if ((shm = shmat(shm_id, NULL, 0)) == (char *) -1) {
		printf("shmat failed.\n");
		return 1;
	}

	memset(shm, 0, shm_size);


#if 0
	dev_size = 0;
	block_size = 0;

	dfd = open(device, O_RDONLY);
	if (dfd < 0) {
		fprintf(stderr, "open(%s) failed: %s\n", device, strerror(errno));
		return 1;
	}

	if (ioctl(dfd, BLKGETSIZE, &dev_size) < 0) {
		fprintf(stderr, "ioctl(BLKGETSIZE64) failed: %s\n", strerror(errno));
		close(dfd);
		return 1;
	}

	if (ioctl(dfd, BLKBSZGET, &block_size) < 0) {
		fprintf(stderr, "ioctl(BLKBSZGET) failed: %s\n", strerror(errno));
		close(dfd);
		return 1;
	}
	close(dfd);
#endif

	dev_size	= dev_size_gb * 1024ULL * 1024ULL * 1024ULL;
	blocks   	= dev_size / block_size;
	p_blocks 	= blocks / procs;

	printf("Device Size = %llu\n", dev_size);
	printf("Device Block Size = %lu\n", block_size);
	printf("Device Blocks = %lu\n", blocks);
	printf("Blocks Per Process = %lu\n", p_blocks);

	buf = malloc(block_size + 1);
	if (!buf) {
		fprintf(stderr, "malloc failed: %s.", strerror(errno));
		goto error;
	}
	memset(buf, 0, block_size + 1);
	fill_buf(buf, block_size, pattern);
	buf[block_size + 1] = 0;

	for (d = 0; d < no_devices; d++) {
		for (i = 0; i < procs; i++) {
			start_block = i * p_blocks;
			end_block   = start_block + p_blocks - 1;

			pid = fork();
			if (pid < 0) {
				fprintf(stderr, "fork failed: %s\n", strerror(errno));
				goto error;
			}

			if (pid == 0) {
				struct thread_data td;
				struct result_data *rd;
				char *device;
				int dfd;

				device = devices[d];

				dfd = open(device, O_RDWR, O_SYNC);
				if (dfd < 0) {
					fprintf(stderr, "open(%s) failed: %s\n", device, strerror(errno));
					return 1;
				}

				/* shared memory attached before fork() are shared by child. No need to reattach */
				rd = (struct result_data *) (shm + (sizeof(struct result_data) * ((procs * d) + i)));
				rd->device_index = d;

				td.start_block	= start_block;
				td.end_block	= end_block;
				td.block_size	= block_size;
				td.iterations	= iterations;
				td.fd		= dfd;
				td.result	= rd;
				td.verify	= verify;
				td.random	= random;
				td.clk_id	= clk_id;
				td.ioengine	= ioengine;

				do_io(&td);
				close(dfd);
				return 0;
			}

			pids[i] = pid;
		}
	}

	if (seconds) {
		unsigned long s = seconds;

		/* sleep for specified time */
		while ((s = sleep(s)));

		/* send kill signal to all children */
		for (i = 0; i < procs; i++) {
			kill(pids[i], SIGKILL);
		}
	}

	for (i = 0; i < procs; i++) {
		waitpid(pids[i], NULL, 0);
	}

	printf("Finished\n");

	rd = (struct result_data *) shm;
	for (d = 0; d < no_devices; d++) {
		avg_write_latency	= 0;
		avg_read_latency	= 0;
		total_writes		= 0;
		total_reads		= 0;
		write_latency		= 0;
		read_latency		= 0;
		read_bw			= 0;
		write_bw		= 0;
		for (i = 0; i < procs; i++) {
			assert (d == rd->device_index);

			write_latency += rd->total_write_latency;
			total_writes  += rd->writes;

			read_latency  += rd->total_read_latency;
			total_reads   += rd->reads;

			rd++;
		}

		printf("\n\nDevice = %s\n", devices[d]);

		if (total_reads)
			avg_read_latency = read_latency / total_reads;

		if (total_writes)
			avg_write_latency = write_latency / total_writes;

		if (avg_read_latency) {
			read_bw = ((double) block_size / (double) avg_read_latency) * SEC_TO_NS * procs;
			read_bw /= (1024 * 1024);
		}

		if (avg_write_latency) {
			write_bw = ((double) block_size / (double) avg_write_latency) * SEC_TO_NS * procs;
			write_bw /= (1024 * 1024);
		}

		printf("avg_read_latency = %lld\n", avg_read_latency);
		printf("Read BW  = %llu MB\n", read_bw);

		printf("avg_write_latency = %lld\n", avg_write_latency);
		printf("Write BW = %llu MB\n", write_bw);
	}

error:
	if (shm)
		shmdt(shm);
	return 0;
}
