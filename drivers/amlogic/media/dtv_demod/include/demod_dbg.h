/*
 * drivers/amlogic/media/dtv_demod/include/demod_dbg.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

struct demod_debugfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

extern void aml_demod_dbg_init(void);
extern void aml_demod_dbg_exit(void);
