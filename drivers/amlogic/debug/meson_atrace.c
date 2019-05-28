/*
 * drivers/amlogic/debug/meson_atrace.c
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
#include <linux/module.h>

#define CREATE_TRACE_POINTS
#include <trace/events/meson_atrace.h>

static uint64_t atrace_tag;

#define TAG_INFO(name) \
	{ #name,  KERNEL_ATRACE_TAG_ ## name }

struct {
	char *name;
	int tag;
} tag_info[] = {
	TAG_INFO(VIDEO),
	TAG_INFO(CODEC_MM),
	TAG_INFO(VDEC),
	TAG_INFO(TSYNC),
	TAG_INFO(IONVIDEO),
	TAG_INFO(AMLVIDEO),
	{ NULL, 0 }
};

int get_atrace_tag_enabled(unsigned short tag)
{
	if (tag == KERNEL_ATRACE_TAG_ALL)
		return 1;
	return atrace_tag & (1 << tag);
}
EXPORT_SYMBOL(get_atrace_tag_enabled);

void set_atrace_tag_enabled(unsigned short tag, int enable)
{
	if (enable)
		atrace_tag |= 1 << tag;
	else
		atrace_tag &= ~(1 << tag);
}
EXPORT_SYMBOL(set_atrace_tag_enabled);

static ssize_t show_atrace_tag(struct class *class,
		struct class_attribute *attr, char *buf)
{
	ssize_t size = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(tag_info) && tag_info[i].name; i++) {
		size += sprintf(buf + size, "%2d %s %s\n",
			tag_info[i].tag, tag_info[i].name,
			get_atrace_tag_enabled(tag_info[i].tag) ? "ON" : "OFF");
	}

	return size;
}

static ssize_t store_atrace_tag(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int tag;
	ssize_t r;

	r = kstrtouint(buf, 0, &tag);

	if (r != 0)
		return -EINVAL;
	if (tag < KERNEL_ATRACE_TAG_MAX)
		set_atrace_tag_enabled(tag, !get_atrace_tag_enabled(tag));

	return size;
}


static struct class_attribute debug_class_attrs[] = {
	__ATTR(atrace_tag, 0664, show_atrace_tag, store_atrace_tag),
	__ATTR_NULL
};

static struct class debug_class = {
		.name = "debug",
		.class_attrs = debug_class_attrs,
	};

static int __init debug_module_init(void)
{
	int r;

	r = class_register(&debug_class);

	if (r) {
		pr_err("debug class create fail.\n");
		return r;
	}

	return 0;
}

static void __exit debug_module_exit(void)
{
	class_unregister(&debug_class);
}

void meson_atrace(int tag, const char *name, unsigned int flags,
	 unsigned int value)
{
	if (get_atrace_tag_enabled(tag))
		trace_tracing_mark_write(name, flags, value);
}
EXPORT_SYMBOL_GPL(meson_atrace);


module_init(debug_module_init);
module_exit(debug_module_exit);
MODULE_DESCRIPTION("AMLOGIC meson debugger controller");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tao Guo <tao.guo@amlogic.com>");
