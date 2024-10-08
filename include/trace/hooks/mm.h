/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mm

#ifdef CREATE_TRACE_POINTS
#define TRACE_INCLUDE_PATH trace/hooks
#define UNDEF_TRACE_INCLUDE_PATH
#endif

#if !defined(_TRACE_HOOK_MM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_MM_H

#include <trace/hooks/vendor_hooks.h>

struct shmem_inode_info;
struct folio;
struct page_vma_mapped_walk;
struct track;

DECLARE_RESTRICTED_HOOK(android_rvh_shmem_get_folio,
			TP_PROTO(struct shmem_inode_info *info, struct folio **folio),
			TP_ARGS(info, folio), 2);

DECLARE_RESTRICTED_HOOK(android_rvh_set_gfp_zone_flags,
			TP_PROTO(unsigned int *flags),	/* gfp_t *flags */
			TP_ARGS(flags), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_set_readahead_gfp_mask,
			TP_PROTO(unsigned int *flags),	/* gfp_t *flags */
			TP_ARGS(flags), 1);
/* This vendor hook is deprecated and is unused. */
DECLARE_RESTRICTED_HOOK(android_rvh_try_alloc_pages,
			TP_PROTO(struct page **page, unsigned int order,
				enum zone_type highest_zoneidx),
			TP_ARGS(page, order, highest_zoneidx), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_try_alloc_pages_gfp,
			TP_PROTO(struct page **page, unsigned int order,
				gfp_t gfp, enum zone_type highest_zoneidx),
			TP_ARGS(page, order, gfp, highest_zoneidx), 1);
DECLARE_HOOK(android_vh_slab_alloc_node,
	TP_PROTO(void *object, unsigned long addr, struct kmem_cache *s),
	TP_ARGS(object, addr, s));
DECLARE_HOOK(android_vh_slab_free,
	TP_PROTO(unsigned long addr, struct kmem_cache *s),
	TP_ARGS(addr, s));
DECLARE_HOOK(android_vh_meminfo_cache_adjust,
	TP_PROTO(unsigned long *cached),
	TP_ARGS(cached));
DECLARE_HOOK(android_vh_si_mem_available_adjust,
	TP_PROTO(unsigned long *available),
	TP_ARGS(available));
DECLARE_HOOK(android_vh_si_meminfo_adjust,
	TP_PROTO(unsigned long *totalram, unsigned long *freeram),
	TP_ARGS(totalram, freeram));
DECLARE_HOOK(android_vh_slab_folio_alloced,
	TP_PROTO(unsigned int order, gfp_t flags),
	TP_ARGS(order, flags));
DECLARE_HOOK(android_vh_process_madvise_begin,
	TP_PROTO(struct task_struct *task, int behavior),
	TP_ARGS(task, behavior));
DECLARE_HOOK(android_vh_process_madvise_iter,
	TP_PROTO(struct task_struct *task, int behavior, ssize_t *ret),
	TP_ARGS(task, behavior, ret));
DECLARE_HOOK(android_vh_kmalloc_large_alloced,
	TP_PROTO(struct page *page, unsigned int order, gfp_t flags),
	TP_ARGS(page, order, flags));
DECLARE_RESTRICTED_HOOK(android_rvh_ctl_dirty_rate,
	TP_PROTO(struct inode *inode),
	TP_ARGS(inode), 1);
DECLARE_HOOK(android_vh_test_clear_look_around_ref,
	TP_PROTO(struct page *page),
	TP_ARGS(page));
DECLARE_HOOK(android_vh_look_around_migrate_folio,
	TP_PROTO(struct folio *old_folio, struct folio *new_folio),
	TP_ARGS(old_folio, new_folio));
DECLARE_HOOK(android_vh_look_around,
	TP_PROTO(struct page_vma_mapped_walk *pvmw, struct folio *folio,
		struct vm_area_struct *vma, int *referenced),
	TP_ARGS(pvmw, folio, vma, referenced));
DECLARE_HOOK(android_vh_mm_kcompactd_cpu_online,
	TP_PROTO(int cpu),
	TP_ARGS(cpu));
DECLARE_HOOK(android_vh_free_unref_page_bypass,
	TP_PROTO(struct page *page, int order, int migratetype, bool *bypass),
	TP_ARGS(page, order, migratetype, bypass));
DECLARE_HOOK(android_vh_kvmalloc_node_use_vmalloc,
	TP_PROTO(size_t size, gfp_t *kmalloc_flags, bool *use_vmalloc),
	TP_ARGS(size, kmalloc_flags, use_vmalloc));
DECLARE_HOOK(android_vh_should_alloc_pages_retry,
	TP_PROTO(gfp_t gfp_mask, int order, int *alloc_flags,
	int migratetype, struct zone *preferred_zone, struct page **page, bool *should_alloc_retry),
	TP_ARGS(gfp_mask, order, alloc_flags,
		migratetype, preferred_zone, page, should_alloc_retry));
DECLARE_HOOK(android_vh_alloc_pages_adjust_wmark,
	TP_PROTO(gfp_t gfp_mask, int order, int *alloc_flags),
	TP_ARGS(gfp_mask, order, alloc_flags));
DECLARE_HOOK(android_vh_alloc_pages_reset_wmark,
	TP_PROTO(gfp_t gfp_mask, int order, int *alloc_flags,
	unsigned long *did_some_progress, int *no_progress_loops,
	unsigned long direct_reclaim_retries),
	TP_ARGS(gfp_mask, order, alloc_flags, did_some_progress,
	no_progress_loops, direct_reclaim_retries));
DECLARE_HOOK(android_vh_unreserve_highatomic_bypass,
	TP_PROTO(bool force, struct zone *zone, bool *skip_unreserve_highatomic),
	TP_ARGS(force, zone, skip_unreserve_highatomic));
DECLARE_HOOK(android_vh_rmqueue_bulk_bypass,
	TP_PROTO(unsigned int order, struct per_cpu_pages *pcp, int migratetype,
		struct list_head *list),
	TP_ARGS(order, pcp, migratetype, list));
DECLARE_HOOK(android_vh_ra_tuning_max_page,
	TP_PROTO(struct readahead_control *ractl, unsigned long *max_page),
	TP_ARGS(ractl, max_page));
DECLARE_HOOK(android_vh_tune_mmap_readaround,
	TP_PROTO(unsigned int ra_pages, pgoff_t pgoff,
		pgoff_t *start, unsigned int *size, unsigned int *async_size),
	TP_ARGS(ra_pages, pgoff, start, size, async_size));
DECLARE_HOOK(android_vh_madvise_cold_pageout_skip,
	TP_PROTO(struct vm_area_struct *vma, struct folio *folio, bool pageout, bool *need_skip),
	TP_ARGS(vma, folio, pageout, need_skip));
struct mem_cgroup;
DECLARE_HOOK(android_vh_mem_cgroup_alloc,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
DECLARE_HOOK(android_vh_mem_cgroup_free,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
DECLARE_HOOK(android_vh_mem_cgroup_id_remove,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
struct cgroup_subsys_state;
DECLARE_HOOK(android_vh_mem_cgroup_css_online,
	TP_PROTO(struct cgroup_subsys_state *css, struct mem_cgroup *memcg),
	TP_ARGS(css, memcg));
DECLARE_HOOK(android_vh_mem_cgroup_css_offline,
	TP_PROTO(struct cgroup_subsys_state *css, struct mem_cgroup *memcg),
	TP_ARGS(css, memcg));
DECLARE_HOOK(android_vh_save_track_hash,
	TP_PROTO(bool alloc, struct track *p),
	TP_ARGS(alloc, p));
DECLARE_HOOK(android_vh_should_fault_around,
	TP_PROTO(struct vm_fault *vmf, bool *should_around),
	TP_ARGS(vmf, should_around));
DECLARE_HOOK(android_vh_kmalloc_slab,
	TP_PROTO(unsigned int index, gfp_t flags, struct kmem_cache **s),
	TP_ARGS(index, flags, s));
DECLARE_HOOK(android_vh_adjust_kvmalloc_flags,
	TP_PROTO(unsigned int order, gfp_t *alloc_flags),
	TP_ARGS(order, alloc_flags));
DECLARE_HOOK(android_vh_alloc_pages_slowpath,
	TP_PROTO(gfp_t gfp_mask, unsigned int order, unsigned long delta),
	TP_ARGS(gfp_mask, order, delta));
DECLARE_HOOK(android_vh_alloc_pages_slowpath_start,
	TP_PROTO(u64 *stime),
	TP_ARGS(stime));
DECLARE_HOOK(android_vh_alloc_pages_slowpath_end,
	TP_PROTO(gfp_t *gfp_mask, unsigned int order, unsigned long alloc_start,
		u64 stime, unsigned long did_some_progress,
		unsigned long pages_reclaimed, int retry_loop_count),
	TP_ARGS(gfp_mask, order, alloc_start, stime, did_some_progress,
		pages_reclaimed, retry_loop_count));
DECLARE_HOOK(android_vh_dm_bufio_shrink_scan_bypass,
	TP_PROTO(unsigned long dm_bufio_current_allocated, bool *bypass),
	TP_ARGS(dm_bufio_current_allocated, bypass));
DECLARE_HOOK(android_vh_cleanup_old_buffers_bypass,
	TP_PROTO(unsigned long dm_bufio_current_allocated,
		unsigned long *max_age_hz,
		bool *bypass),
	TP_ARGS(dm_bufio_current_allocated, max_age_hz, bypass));
DECLARE_HOOK(android_vh_mmap_region,
	TP_PROTO(struct vm_area_struct *vma, unsigned long addr),
	TP_ARGS(vma, addr));
DECLARE_HOOK(android_vh_try_to_unmap_one,
	TP_PROTO(struct folio *folio, struct vm_area_struct *vma,
		unsigned long addr, void *arg, bool ret),
	TP_ARGS(folio, vma, addr, arg, ret));
DECLARE_HOOK(android_vh_mm_direct_reclaim_enter,
	TP_PROTO(unsigned int order),
	TP_ARGS(order));
DECLARE_HOOK(android_vh_mm_direct_reclaim_exit,
	TP_PROTO(unsigned long did_some_progress, int retry_times),
	TP_ARGS(did_some_progress, retry_times));
struct oom_control;
DECLARE_HOOK(android_vh_mm_may_oom_exit,
	TP_PROTO(struct oom_control *oc, unsigned long did_some_progress),
	TP_ARGS(oc, did_some_progress));
DECLARE_HOOK(android_vh_do_anonymous_page,
	TP_PROTO(struct vm_area_struct *vma, struct folio *folio),
	TP_ARGS(vma, folio));
DECLARE_HOOK(android_vh_do_swap_page,
	TP_PROTO(struct folio *folio, pte_t *pte, struct vm_fault *vmf,
		swp_entry_t entry),
	TP_ARGS(folio, pte, vmf, entry));
DECLARE_HOOK(android_vh_do_wp_page,
	TP_PROTO(struct folio *folio),
	TP_ARGS(folio));
DECLARE_HOOK(android_vh_uprobes_replace_page,
	TP_PROTO(struct folio *new_folio, struct folio *old_folio),
	TP_ARGS(new_folio, old_folio));
DECLARE_HOOK(android_vh_shmem_swapin_folio,
	TP_PROTO(struct folio *folio),
	TP_ARGS(folio));
DECLARE_HOOK(android_vh_get_page_wmark,
	TP_PROTO(unsigned int alloc_flags, unsigned long *page_wmark),
	TP_ARGS(alloc_flags, page_wmark));
DECLARE_HOOK(android_vh_page_add_new_anon_rmap,
	TP_PROTO(struct page *page, struct vm_area_struct *vma,
		unsigned long address),
	TP_ARGS(page, vma, address));
DECLARE_HOOK(android_vh_meminfo_proc_show,
	TP_PROTO(struct seq_file *m),
	TP_ARGS(m));
DECLARE_HOOK(android_vh_exit_mm,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_show_mem,
	TP_PROTO(unsigned int filter, nodemask_t *nodemask),
	TP_ARGS(filter, nodemask));
DECLARE_HOOK(android_vh_print_slabinfo_header,
	TP_PROTO(struct seq_file *m),
	TP_ARGS(m));
struct slabinfo;
DECLARE_HOOK(android_vh_cache_show,
	TP_PROTO(struct seq_file *m, struct slabinfo *sinfo, struct kmem_cache *s),
	TP_ARGS(m, sinfo, s));
DECLARE_HOOK(android_vh_customize_alloc_gfp,
	TP_PROTO(gfp_t *alloc_gfp, unsigned int order),
	TP_ARGS(alloc_gfp, order));
DECLARE_HOOK(android_vh_madvise_pageout_swap_entry,
	TP_PROTO(swp_entry_t entry, int swapcount, void *priv),
	TP_ARGS(entry, swapcount, priv));
DECLARE_HOOK(android_vh_madvise_swapin_walk_pmd_entry,
	TP_PROTO(swp_entry_t entry),
	TP_ARGS(entry));
DECLARE_HOOK(android_vh_process_madvise,
	TP_PROTO(int behavior, ssize_t *ret, void *priv),
	TP_ARGS(behavior, ret, priv));
DECLARE_HOOK(android_vh_smaps_pte_entry,
	TP_PROTO(swp_entry_t entry, unsigned long *writeback,
		unsigned long *same, unsigned long *huge),
	TP_ARGS(entry, writeback, same, huge));
DECLARE_HOOK(android_vh_show_smap,
	TP_PROTO(struct seq_file *m, unsigned long writeback,
		unsigned long same, unsigned long huge),
	TP_ARGS(m, writeback, same, huge));
DECLARE_HOOK(android_vh_smaps_swap_shared,
	TP_PROTO(unsigned long *swap_shared),
	TP_ARGS(swap_shared));
DECLARE_HOOK(android_vh_show_smap_swap_shared,
	TP_PROTO(struct seq_file *m, unsigned long swap_shared),
	TP_ARGS(m, swap_shared));
DECLARE_HOOK(android_vh_count_workingset_refault,
	TP_PROTO(struct folio *folio),
	TP_ARGS(folio));
DECLARE_HOOK(android_vh_alloc_pages_reclaim_bypass,
    TP_PROTO(gfp_t gfp_mask, int order, int alloc_flags,
	int migratetype, struct page **page),
	TP_ARGS(gfp_mask, order, alloc_flags, migratetype, page));
DECLARE_HOOK(android_vh_alloc_pages_failure_bypass,
	TP_PROTO(gfp_t gfp_mask, int order, int alloc_flags,
	int migratetype, struct page **page),
	TP_ARGS(gfp_mask, order, alloc_flags, migratetype, page));
DECLARE_HOOK(android_vh_swapmem_gather_init,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_swapmem_gather_add_bypass,
	TP_PROTO(struct mm_struct *mm, swp_entry_t entry, int nr, bool *bypass),
	TP_ARGS(mm, entry, nr, bypass));
DECLARE_HOOK(android_vh_swapmem_gather_finish,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_oom_swapmem_gather_init,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_oom_swapmem_gather_finish,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_drain_all_pages_bypass,
	TP_PROTO(gfp_t gfp_mask, unsigned int order, unsigned long alloc_flags,
		int migratetype, unsigned long did_some_progress,
		bool *bypass),
	TP_ARGS(gfp_mask, order, alloc_flags, migratetype, did_some_progress, bypass));
DECLARE_HOOK(android_vh_save_vmalloc_stack,
	TP_PROTO(unsigned long flags, struct vm_struct *vm),
	TP_ARGS(flags, vm));
DECLARE_HOOK(android_vh_show_stack_hash,
	TP_PROTO(struct seq_file *m, struct vm_struct *v),
	TP_ARGS(m, v));
DECLARE_HOOK(android_vh_update_page_mapcount,
	TP_PROTO(struct page *page, bool inc_size, bool compound,
			int *first_mapping, bool *success),
	TP_ARGS(page, inc_size, compound, first_mapping, success));
DECLARE_HOOK(android_vh_add_page_to_lrulist,
	TP_PROTO(struct folio *folio, bool compound, enum lru_list lru),
	TP_ARGS(folio, compound, lru));
DECLARE_HOOK(android_vh_del_page_from_lrulist,
	TP_PROTO(struct folio *folio, bool compound, enum lru_list lru),
	TP_ARGS(folio, compound, lru));
DECLARE_HOOK(android_vh_show_mapcount_pages,
	TP_PROTO(void *unused),
	TP_ARGS(unused));
DECLARE_HOOK(android_vh_do_traversal_lruvec,
	TP_PROTO(struct lruvec *lruvec),
	TP_ARGS(lruvec));
DECLARE_HOOK(android_vh_page_should_be_protected,
	TP_PROTO(struct folio *folio, unsigned long nr_scanned,
	s8 priority, u64 *ext, int *should_protect),
	TP_ARGS(folio, nr_scanned, priority, ext, should_protect));
DECLARE_HOOK(android_vh_mark_page_accessed,
	TP_PROTO(struct folio *folio),
	TP_ARGS(folio));

DECLARE_HOOK(android_vh_lock_folio_drop_mmap_start,
	TP_PROTO(struct task_struct **tsk, struct vm_fault *vmf,
		struct folio *folio, struct file *file),
	TP_ARGS(tsk, vmf, folio, file));

DECLARE_HOOK(android_vh_lock_folio_drop_mmap_end,
	TP_PROTO(bool success, struct task_struct **tsk, struct vm_fault *vmf,
		struct folio *folio, struct file *file),
	TP_ARGS(success, tsk, vmf, folio, file));

DECLARE_HOOK(android_vh_filemap_update_page,
	TP_PROTO(struct address_space *mapping, struct folio *folio,
		struct file *file),
	TP_ARGS(mapping, folio, file));
DECLARE_HOOK(android_vh_cma_debug_show_areas,
	TP_PROTO(bool *show),
	TP_ARGS(show));
DECLARE_HOOK(android_vh_alloc_contig_range_not_isolated,
	TP_PROTO(unsigned long start, unsigned end),
	TP_ARGS(start, end));
DECLARE_HOOK(android_vh_warn_alloc_tune_ratelimit,
	TP_PROTO(struct ratelimit_state *rs),
	TP_ARGS(rs));
DECLARE_HOOK(android_vh_warn_alloc_show_mem_bypass,
	TP_PROTO(bool *bypass),
	TP_ARGS(bypass));
DECLARE_HOOK(android_vh_free_pages_prepare_bypass,
	TP_PROTO(struct page *page, unsigned int order,
		int __bitwise flags, bool *skip_free_pages_prepare),
	TP_ARGS(page, order, flags, skip_free_pages_prepare));
DECLARE_HOOK(android_vh_free_pages_ok_bypass,
	TP_PROTO(struct page *page, unsigned int order,
		int __bitwise flags, bool *skip_free_pages_ok),
	TP_ARGS(page, order, flags, skip_free_pages_ok));
DECLARE_HOOK(android_vh_split_large_folio_bypass,
	TP_PROTO(bool *bypass),
	TP_ARGS(bypass));
DECLARE_HOOK(android_vh_do_read_fault,
	TP_PROTO(struct vm_fault *vmf, unsigned long fault_around_bytes),
	TP_ARGS(vmf, fault_around_bytes));
DECLARE_HOOK(android_vh_filemap_read,
	TP_PROTO(struct file *file, loff_t pos, size_t size),
	TP_ARGS(file, pos, size));
DECLARE_HOOK(android_vh_filemap_map_pages,
	TP_PROTO(struct file *file, pgoff_t first_pgoff,
		pgoff_t last_pgoff, vm_fault_t ret),
	TP_ARGS(file, first_pgoff, last_pgoff, ret));
DECLARE_HOOK(android_vh_page_cache_readahead_start,
	TP_PROTO(struct file *file, pgoff_t pgoff,
		unsigned int size, bool sync),
	TP_ARGS(file, pgoff, size, sync));
DECLARE_HOOK(android_vh_page_cache_readahead_end,
	TP_PROTO(struct file *file, pgoff_t pgoff),
	TP_ARGS(file, pgoff));
DECLARE_HOOK(android_vh_filemap_fault_start,
	TP_PROTO(struct file *file, pgoff_t pgoff),
	TP_ARGS(file, pgoff));
DECLARE_HOOK(android_vh_filemap_fault_end,
	TP_PROTO(struct file *file, pgoff_t pgoff),
	TP_ARGS(file, pgoff));

DECLARE_HOOK(android_vh_cma_alloc_set_max_retries,
	TP_PROTO(int *max_retries),
	TP_ARGS(max_retries));
DECLARE_HOOK(android_vh_compact_finished,
	TP_PROTO(bool *abort_compact),
	TP_ARGS(abort_compact));
DECLARE_HOOK(android_vh_madvise_cold_or_pageout_abort,
	TP_PROTO(struct vm_area_struct *vma, bool *abort_madvise),
	TP_ARGS(vma, abort_madvise));
DECLARE_HOOK(android_vh_zs_shrinker_adjust,
	TP_PROTO(unsigned long *pages_to_free),
	TP_ARGS(pages_to_free));
DECLARE_HOOK(android_vh_zs_shrinker_bypass,
	TP_PROTO(bool *bypass),
	TP_ARGS(bypass));
DECLARE_HOOK(android_vh_customize_thp_pcp_order,
	TP_PROTO(unsigned int *order),
	TP_ARGS(order));
DECLARE_HOOK(android_vh_customize_thp_gfp_orders,
	TP_PROTO(gfp_t *gfp_mask, unsigned long *orders, int *order),
	TP_ARGS(gfp_mask, orders, order));
DECLARE_HOOK(android_vh_init_adjust_zone_wmark,
	TP_PROTO(struct zone *zone, u64 interval),
	TP_ARGS(zone, interval));
DECLARE_HOOK(android_vh_cma_alloc_retry,
	TP_PROTO(char *name, int *retry),
	TP_ARGS(name, retry));
DECLARE_HOOK(android_vh_do_group_exit,
	TP_PROTO(struct task_struct *tsk),
	TP_ARGS(tsk));
DECLARE_HOOK(android_vh_suitable_migration_target_bypass,
	TP_PROTO(struct page *page, bool *bypass),
	TP_ARGS(page, bypass));
DECLARE_HOOK(android_vh_swap_writepage,
	TP_PROTO(unsigned long *sis_flags, struct page *page),
	TP_ARGS(sis_flags, page));
#endif /* _TRACE_HOOK_MM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
