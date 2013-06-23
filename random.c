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
