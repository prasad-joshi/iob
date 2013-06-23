#ifndef __RANDOM_H__
#define __RANDOM_H__

struct rand_range {
	unsigned int seed_w;
	unsigned int seed_z;

	unsigned long start;
	unsigned long end;
};

void init_rand_range(struct rand_range *r, unsigned long start, unsigned long end);

unsigned long get_random_range(struct rand_range *r);
#endif
