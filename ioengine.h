#ifndef __IOENGINE_H__
#define __IOENGINE_H__

#define IOENGIN_NAME_LENGTH 8

struct ioengine {
	char name[IOENGIN_NAME_LENGTH + 1];
	int (*read_block)(int fd, void *buf, unsigned long block,
			unsigned long block_size);
	int (*write_block)(int fd, void *buf, unsigned long block,
			unsigned long block_size);
};

#endif
