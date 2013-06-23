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
