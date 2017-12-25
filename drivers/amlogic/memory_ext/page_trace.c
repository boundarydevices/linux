/*
 * drivers/amlogic/memory_ext/page_trace.c
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

#include <linux/amlogic/page_trace.h>
#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/mmzone.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/sort.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <asm/stacktrace.h>

#define DEBUG_PAGE_TRACE	0

#define COMMON_CALLER_SIZE	20

/*
 * this is a driver which will hook during page alloc/free and
 * record caller of each page to a buffer. Detailed information
 * of page allocate statistics can be find in /proc/pagetrace
 *
 */
static unsigned int trace_step = 1;
static bool merge_function;
struct page_trace *trace_buffer;
static unsigned long ptrace_size;
static struct proc_dir_entry *dentry;
static bool page_trace_disable __initdata = 1;

struct alloc_caller {
	unsigned long func_start_addr;
	unsigned long size;
};

struct fun_symbol {
	const char *name;
	int full_match;
};

static struct alloc_caller common_caller[COMMON_CALLER_SIZE];

/*
 * following functions are common API from page allocate, they should not
 * be record in page trace, parse for back trace should keep from these
 * functions
 */
static struct fun_symbol common_func[] __initdata = {
	{"__alloc_pages_nodemask",	1},
	{"kmem_cache_alloc",		1},
	{"__get_free_pages",		1},
	{"__kmalloc",			1},
	{"cma_alloc",			1},
	{"dma_alloc_from_contiguous",	1},
	{"__dma_alloc",			1},
	{"__kmalloc_track_caller",	1},
	{"kmem_cache_alloc_trace",	1},
	{"alloc_pages_exact",		1},
	{"__alloc_page_frag",		1},
	{"kmalloc_order",		0},
#ifdef CONFIG_SLUB	/* for some static symbols not exported in headfile */
	{"new_slab",			0},
	{"slab_alloc",			0},
#endif
	{}		/* tail */
};

static int early_page_trace_param(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (strcmp(buf, "off") == 0)
		page_trace_disable = true;
	else if (strcmp(buf, "on") == 0)
		page_trace_disable = false;

	pr_info("page_trace %sabled\n", page_trace_disable ? "dis" : "en");

	return 0;
}
early_param("page_trace", early_page_trace_param);

static int early_page_trace_step(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &trace_step))
		pr_info("page trace_step:%d\n", trace_step);

	return 0;
}
early_param("page_trace_step", early_page_trace_step);

static inline bool page_trace_invalid(struct page_trace *trace)
{
	return trace->order == IP_INVALID;
}

#if DEBUG_PAGE_TRACE
static inline bool range_ok(struct page_trace *trace)
{
	unsigned long offset;

	offset = (trace->ret_ip << 2);
	if (trace->module_flag) {
		if (offset >= MODULES_END)
			return false;
	} else {
		if (offset >= ((unsigned long)_end - (unsigned long)_text))
			return false;
	}
	return true;
}

static bool check_trace_valid(struct page_trace *trace)
{
	unsigned long offset;

	if (trace->order == IP_INVALID)
		return true;

	if (trace->order >= 10 ||
	    trace->migrate_type >= MIGRATE_TYPES ||
	    !range_ok(trace)) {
		offset = (unsigned long)((trace - trace_buffer));
		pr_err("bad trace:%p, %x, pfn:%lx, ip:%pf\n", trace,
			*((unsigned int *)trace),
			offset / sizeof(struct page_trace),
			(void *)_RET_IP_);
		return false;
	}
	return true;
}
#else
static inline bool check_trace_valid(struct page_trace *trace)
{
	return true;
}
#endif /* DEBUG_PAGE_TRACE */

static void push_ip(struct page_trace *base, struct page_trace *ip)
{
	int i;
	unsigned long end;

	for (i = trace_step - 1; i > 0; i--)
		base[i] = base[i - 1];

	/* debug check */
	check_trace_valid(base);
	end = (((unsigned long)trace_buffer) + ptrace_size);
	WARN_ON((unsigned long)(base + trace_step - 1) >= end);

	base[0] = *ip;
}

static inline int is_module_addr(unsigned long ip)
{
	if (ip >= MODULES_VADDR && ip < MODULES_END)
		return 1;
	return 0;
}

/*
 * following 3 functions are modify from kernel/kallsyms.c
 */
static unsigned int __init kallsyms_expand_symbol(unsigned int off,
					   char *result, size_t maxlen)
{
	int len, skipped_first = 0;
	const u8 *tptr, *data;

	/* Get the compressed symbol length from the first symbol byte. */
	data = &kallsyms_names[off];
	len = *data;
	data++;

	/*
	 * Update the offset to return the offset for the next symbol on
	 * the compressed stream.
	 */
	off += len + 1;

	/*
	 * For every byte on the compressed symbol data, copy the table
	 * entry for that byte.
	 */
	while (len) {
		tptr = &kallsyms_token_table[kallsyms_token_index[*data]];
		data++;
		len--;

		while (*tptr) {
			if (skipped_first) {
				if (maxlen <= 1)
					goto tail;
				*result = *tptr;
				result++;
				maxlen--;
			} else
				skipped_first = 1;
			tptr++;
		}
	}

tail:
	if (maxlen)
		*result = '\0';

	/* Return to offset to the next symbol. */
	return off;
}

static unsigned long __init kallsyms_sym_address(int idx)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return kallsyms_addresses[idx];

	/* values are unsigned offsets if --absolute-percpu is not in effect */
	if (!IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU))
		return kallsyms_relative_base + (u32)kallsyms_offsets[idx];

	/* ...otherwise, positive offsets are absolute values */
	if (kallsyms_offsets[idx] >= 0)
		return kallsyms_offsets[idx];

	/* ...and negative offsets are relative to kallsyms_relative_base - 1 */
	return kallsyms_relative_base - 1 - kallsyms_offsets[idx];
}

/* Lookup the address for this symbol. Returns 0 if not found. */
static unsigned long __init kallsyms_contain_name(const char *name, long full,
						  unsigned int *offset)
{
	char namebuf[KSYM_NAME_LEN];
	unsigned long i;
	unsigned int off = 0;

	for (i = 0; i < kallsyms_num_syms; i++) {
		off = kallsyms_expand_symbol(off, namebuf, ARRAY_SIZE(namebuf));

		if (full && strcmp(namebuf, name) == 0)
			return kallsyms_sym_address(i);
		if (!full && strstr(namebuf, name) && (off > *offset)) {
			*offset = off;	/* update offset for next loop */
			return kallsyms_sym_address(i);
		}
	}
	return 0;
}

/*
 * set up information for common caller in memory allocate API
 */
static void __init setup_common_caller(unsigned long kaddr)
{
	unsigned long size, offset;
	int i = 0;
	const char *name;
	char str[KSYM_NAME_LEN];

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		/* find a empty caller */
		if (!common_caller[i].func_start_addr)
			break;
	}
	if (i >= COMMON_CALLER_SIZE) {
		pr_err("%s, out of memory\n", __func__);
		return;
	}

	name = kallsyms_lookup(kaddr, &size, &offset, NULL, str);
	if (name) {
		common_caller[i].func_start_addr = kaddr;
		common_caller[i].size = size;
		pr_debug("setup %d caller:%lx + %lx, %pf\n",
			i, kaddr, size, (void *)kaddr);
	} else
		pr_err("can't find symbol %pf\n", (void *)kaddr);
}

static int __init fuzzy_match(const char *name)
{
	unsigned int off = 0;
	unsigned long addr;
	int find = 0;

	while (1) {
		addr = kallsyms_contain_name(name, 0, &off);
		if (addr) {
			setup_common_caller(addr);
			find++;
		} else
			break;
	}
	return find;
}

static void __init dump_common_caller(void)
{
	int i;

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		if (common_caller[i].func_start_addr)
			pr_debug("%2d, addr:%lx + %4lx, %pf\n", i,
				common_caller[i].func_start_addr,
				common_caller[i].size,
				(void *)common_caller[i].func_start_addr);
		else
			break;
	}
}

static void __init find_static_common_symbol(void)
{
	int i;
	unsigned long addr;
	struct fun_symbol *s;

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		s = &common_func[i];
		if (!s->name)
			break;	/* end */
		if (s->full_match) {
			addr = kallsyms_contain_name(s->name, 1, NULL);
			if (addr)
				setup_common_caller(addr);
			else
				pr_info("can't find symbol:%s\n", s->name);
		} else {
			if (!fuzzy_match(s->name))
				pr_info("can't fuzzy match:%s\n", s->name);
		}
	}
	dump_common_caller();
}

static int is_common_caller(struct alloc_caller *caller, unsigned long pc)
{
	int i, ret = 0;

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		if (!caller[i].func_start_addr)	/* end if this table */
			break;

		/* pc is in one of common caller */
		if ((pc >= caller[i].func_start_addr) &&
		    (pc <= (caller[i].func_start_addr + caller[i].size))) {
			ret = 1;
			break;
		}
	}
	return ret;
}

unsigned long unpack_ip(struct page_trace *trace)
{
	unsigned long text;

	if (trace->module_flag)
		text = MODULES_VADDR;
	else
		text = (unsigned long)_text;
	return text + ((trace->ret_ip) << 2);
}
EXPORT_SYMBOL(unpack_ip);

static inline unsigned long find_back_trace(void)
{
	struct stackframe frame;
	int ret, step = 0;

	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_stack_pointer;
	frame.pc = _RET_IP_;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = current->curr_ret_stack;
#endif
	while (1) {
	#ifdef CONFIG_ARM64
		ret = unwind_frame(current, &frame);
	#elif defined(CONFIG_ARM)
		ret = unwind_frame(&frame);
	#else	/* not supported */
		ret = -1;
	#endif
		if (ret < 0) {
			pr_err("%s, can't find back trace\n", __func__);
			return 0;
		}
		step++;
		if (!is_common_caller(common_caller, frame.pc) && step > 1)
			return frame.pc;
	}
	pr_info("can't get pc\n");
	dump_stack();
	return 0;
}

struct page_trace *find_page_base(struct page *page)
{
	unsigned long pfn, zone_offset = 0, offset;
	struct zone *zone;
	struct page_trace *p;

	pfn = page_to_pfn(page);
	for_each_populated_zone(zone) {
		/* pfn is in this zone */
		if (pfn <= zone_end_pfn(zone) &&
		    pfn >= zone->zone_start_pfn) {
			offset = pfn - zone->zone_start_pfn;
			p = trace_buffer;
			return p + ((offset + zone_offset) * trace_step);
		}
		/* next zone */
		zone_offset += zone->spanned_pages;
	}
	return NULL;
}

static void __init set_init_page_trace(struct page *page, int order, gfp_t flag)
{
	unsigned long text, ip;
	struct page_trace trace = {}, *base;

	if (page && trace_buffer) {
		ip = (unsigned long)set_page_trace;
		text = (unsigned long)_text;

		trace.ret_ip = (ip - text) >> 2;
		WARN_ON(trace.ret_ip > IP_RANGE_MASK);
		trace.migrate_type = gfpflags_to_migratetype(flag);
		trace.order = order;
		base = find_page_base(page);
		push_ip(base, &trace);
	}

}

void set_page_trace(struct page *page, int order, gfp_t gfp_flags)
{
	unsigned long text, ip;
	struct page_trace trace = {}, *base;

	if (page && trace_buffer) {
		ip = find_back_trace();
		if (!ip) {
			pr_err("can't find backtrace for page:%lx\n",
				page_to_pfn(page));
			dump_stack();
			return;
		}
		text = (unsigned long)_text;
		if (ip >= (unsigned long)_text)
			text = (unsigned long)_text;
		else if (is_module_addr(ip)) {
			text = MODULES_VADDR;
			trace.module_flag = 1;
		}

		trace.ret_ip = (ip - text) >> 2;
		WARN_ON(trace.ret_ip > IP_RANGE_MASK);
		if (gfp_flags == __GFP_BDEV)
			trace.migrate_type = MIGRATE_CMA;
		else
			trace.migrate_type = gfpflags_to_migratetype(gfp_flags);
		trace.order = order;
		base = find_page_base(page);
	#if DEBUG_PAGE_TRACE
		pr_debug("%s, base:%p, page:%lx, _ip:%x, o:%d, f:%x, ip:%lx\n",
			 __func__, base, page_to_pfn(page),
			 (*((unsigned int *)&trace)), order,
			 gfp_flags, ip);
	#endif
		push_ip(base, &trace);
	}
}
EXPORT_SYMBOL(set_page_trace);

void reset_page_trace(struct page *page, int order)
{
	struct page_trace *base;
	int i, cnt;
#if DEBUG_PAGE_TRACE
	unsigned long end;
#endif

	if (page && trace_buffer) {
		base = find_page_base(page);
		cnt = 1 << order;
	#if DEBUG_PAGE_TRACE
		check_trace_valid(base);
		end = ((unsigned long)trace_buffer + ptrace_size);
		WARN_ON((unsigned long)(base + cnt * trace_step - 1) >= end);
	#endif
		for (i = 0; i < cnt; i++) {
			base->order = IP_INVALID;
			base += (trace_step);
		}
	}
}
EXPORT_SYMBOL(reset_page_trace);

/*
 * move page out of buddy and make sure they are not malloced by
 * other module
 *
 * Note:
 * before call this functions, memory for page trace buffer are
 * freed to buddy.
 */
static int __init page_trace_pre_work(unsigned long size)
{
	unsigned int order = get_order(size);
	unsigned long addr;
	unsigned long used, end;
	struct page *page;

	if (order >= MAX_ORDER) {
		pr_err("trace buffer size %lx is too large\n", size);
		return -1;
	}
	addr = __get_free_pages(GFP_KERNEL, order);
	if (!addr)
		return -ENOMEM;

	trace_buffer = (struct page_trace *)addr;
	end  = addr + (PAGE_SIZE << order);
	used = addr + size;
	pr_info("%s, trace buffer:%p, size:%lx, used:%lx, end:%lx\n",
		__func__, trace_buffer, size, used, end);
	memset((void *)addr, 0, size);

	for (; addr < end; addr += PAGE_SIZE) {
		if (addr < used) {
			page = virt_to_page((void *)addr);
			set_init_page_trace(page, 0, GFP_KERNEL);
		#if DEBUG_PAGE_TRACE
			pr_info("%s, trace page:%p, %lx\n",
				__func__, page, page_to_pfn(page));
		#endif
		} else {
			free_page(addr);
		#if DEBUG_PAGE_TRACE
			pr_info("%s, free page:%lx, %lx\n", __func__,
				addr, page_to_pfn(virt_to_page(addr)));
		#endif
		}
	}
	return 0;
}

#define SHOW_CNT	1024
struct page_summary {
	unsigned long ip;
	unsigned int cnt;
};

static unsigned long find_ip_base(unsigned long ip)
{
	unsigned long size, offset;
	const char *name;
	char str[KSYM_NAME_LEN];

	name = kallsyms_lookup(ip, &size, &offset, NULL, str);
	if (name)	/* find */
		return ip - offset;
	else		/* not find */
		return ip;
}

static int find_page_ip(struct page_trace *trace,
			struct page_summary *sum, int *o)
{
	int i = 0;
	int order;
	unsigned long ip;

	*o = 0;
	ip = unpack_ip(trace);
	order = trace->order;
	if (merge_function)
		ip = find_ip_base(ip);
	for (i = 0; i < SHOW_CNT; i++) {
		if (sum[i].ip == ip) {
			/* find */
			sum[i].cnt += (1 << order);
			*o = order;
			return 0;
		}
		if (sum[i].ip == 0) {			/* empty */
			sum[i].cnt += (1 << order);
			sum[i].ip = ip;
			*o = order;
			return 1;
		}
	}
	return 0;
}

#define K(x)		((x) << (PAGE_SHIFT - 10))
static int trace_cmp(const void *x1, const void *x2)
{
	struct page_summary *s1, *s2;

	s1 = (struct page_summary *)x1;
	s2 = (struct page_summary *)x2;
	return s2->cnt - s1->cnt;
}

static void show_page_trace(struct seq_file *m,
			    struct page_summary *sum, int cnt, int type)
{
	int i;
	unsigned long total = 0;

	if (!cnt)
		return;
	sort(sum, cnt, sizeof(*sum), trace_cmp, NULL);
	for (i = 0; i < cnt; i++) {
		seq_printf(m, "%8d, %16lx, %pf\n",
			   K(sum[i].cnt), sum[i].ip, (void *)sum[i].ip);
		total += sum[i].cnt;
	}
	seq_puts(m, "------------------------------\n");
	seq_printf(m, "total pages:%ld, %ld kB, type:%s\n",
		   total, K(total), migratetype_names[type]);
}

static inline int type_match(struct page_trace *trace, int type)
{
	return (trace->migrate_type) == type;
}

static int update_page_trace(struct seq_file *m, struct zone *zone,
			     struct page_summary *sum, int type)
{
	unsigned long pfn, flags;
	unsigned long start_pfn = zone->zone_start_pfn;
	unsigned long end_pfn = zone_end_pfn(zone);
	int    max_trace = 0, ret;
	int    order;
	unsigned long ip;
	struct page_trace *trace;

	spin_lock_irqsave(&zone->lock, flags);
	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		struct page *page;

		if (!pfn_valid(pfn))
			continue;

		page = pfn_to_page(pfn);

		/* Watch for unexpected holes punched in the memmap */
		if (!memmap_valid_within(pfn, page, zone))
			continue;

		trace = find_page_base(page);
		check_trace_valid(trace);
		if (page_trace_invalid(trace)) /* free pages */
			continue;

		if (!(*(unsigned int *)trace)) /* empty */
			continue;

		if (type_match(trace, type)) {
			ret = find_page_ip(trace, sum, &order);
			if (max_trace == SHOW_CNT && ret) {
				ip = unpack_ip(trace);
				pr_err("MAX sum cnt, pfn:%ld, lr:%lx, %pf\n",
				       pfn, ip, (void *)ip);
			} else
				max_trace += ret;
			if (order)
				pfn += ((1 << order) - 1);
		}
	}
	spin_unlock_irqrestore(&zone->lock, flags);
	return max_trace;
}
/*
 * This prints out statistics in relation to grouping pages by mobility.
 * It is expensive to collect so do not constantly read the file.
 */
static int pagetrace_show(struct seq_file *m, void *arg)
{
	pg_data_t *p = (pg_data_t *)arg;
	struct zone *zone;
	int mtype, ret, print_flag;
	struct page_summary *sum;

	/* check memoryless node */
	if (!node_state(p->node_id, N_MEMORY))
		return 0;

	sum = vmalloc(sizeof(struct page_summary) * SHOW_CNT);
	if (!sum)
		return -ENOMEM;

	for_each_populated_zone(zone) {
		print_flag = 0;
		seq_printf(m, "Node %d, zone %8s\n", p->node_id, zone->name);
		for (mtype = 0; mtype < MIGRATE_TYPES; mtype++) {
			memset(sum, 0, sizeof(struct page_summary) * SHOW_CNT);
			ret = update_page_trace(m, zone, sum, mtype);
			if (ret > 0) {
				seq_printf(m, "%s %s            %s\n",
					   "count(KB)", "kaddr", "function");
				seq_puts(m, "------------------------------\n");
				show_page_trace(m, sum, ret, mtype);
				seq_puts(m, "\n");
				print_flag = 1;
			}
		}
		if (print_flag)
			seq_puts(m, "------------------------------\n");
	}
	vfree(sum);
	return 0;
}

static void *frag_start(struct seq_file *m, loff_t *pos)
{
	pg_data_t *pgdat;
	loff_t node = *pos;

	for (pgdat = first_online_pgdat();
	     pgdat && node;
	     pgdat = next_online_pgdat(pgdat))
		--node;

	return pgdat;
}

static void *frag_next(struct seq_file *m, void *arg, loff_t *pos)
{
	pg_data_t *pgdat = (pg_data_t *)arg;

	(*pos)++;
	return next_online_pgdat(pgdat);
}

static void frag_stop(struct seq_file *m, void *arg)
{
}
static const struct seq_operations pagetrace_op = {
	.start	= frag_start,
	.next	= frag_next,
	.stop	= frag_stop,
	.show	= pagetrace_show,
};

static int pagetrace_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &pagetrace_op);
}

static ssize_t pagetrace_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *ppos)
{
	char *buf;
	unsigned long arg = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EINVAL;
	}

	if (!strncmp(buf, "merge=", 6)) {	/* option for 'merge=' */
		if (sscanf(buf, "merge=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		merge_function = arg ? 1 : 0;
		pr_info("set merge_function to %d\n", merge_function);
	}

	kfree(buf);

	return count;
}

static const struct file_operations pagetrace_file_ops = {
	.open		= pagetrace_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= pagetrace_write,
	.release	= seq_release,
};

static int __init page_trace_module_init(void)
{
	if (!trace_buffer)
		return -ENOMEM;

	dentry = proc_create("pagetrace", 0444, NULL, &pagetrace_file_ops);
	if (IS_ERR_OR_NULL(dentry)) {
		pr_err("%s, create sysfs failed\n", __func__);
		return -1;
	}

	return 0;
}

static void __exit page_trace_module_exit(void)
{
	if (dentry)
		proc_remove(dentry);
}
module_init(page_trace_module_init);
module_exit(page_trace_module_exit);

void __init page_trace_mem_init(void)
{
	struct zone *zone;
	unsigned long total_page = 0;

	if (page_trace_disable)
		return;

	for_each_populated_zone(zone) {
		total_page += zone->spanned_pages;
		pr_info("zone:%s, spaned pages:%ld, total:%ld\n",
			zone->name, zone->spanned_pages, total_page);
	}
	ptrace_size = total_page * sizeof(struct page_trace) * trace_step;
	ptrace_size = PAGE_ALIGN(ptrace_size);
	if (page_trace_pre_work(ptrace_size)) {
		trace_buffer = NULL;
		ptrace_size = 0;
		pr_err("%s reserve memory failed\n", __func__);
		return;
	}
	find_static_common_symbol();
}

