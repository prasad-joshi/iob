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
