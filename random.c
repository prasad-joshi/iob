#include "random.h"

static void seed_random(struct rand_range *r)
{
	r->seed_w = 100;
	r->seed_z = 100;
}

void init_rand_range(struct rand_range *r, unsigned long start, unsigned long end)
{
	seed_random(r);
	r->start = start;
	r->end   = end;
}

static unsigned int get_random(struct rand_range *r)
{
	unsigned int no;
	unsigned int mz, mw;

	mz = r->seed_z;
	mw = r->seed_w;

	mz = 36969 * (mz & 65535) + (mz >> 16);
	mw = 18000 * (mw & 65535) + (mw >> 16);

	r->seed_w = mw;
	r->seed_z = mz;
	return no;
}

unsigned long get_random_range(struct rand_range *r)
{
	unsigned int no;

	no = rand();

	no = no % (r->end - r->start + 1);
	return r->start + no;
}
