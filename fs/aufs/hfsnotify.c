/*
 * Copyright (C) 2005-2011 Junjiro R. Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 * fsnotify for the lower directories
 */

#include "aufs.h"

/* FS_IN_IGNORED is unnecessary */
static const __u32 AuHfsnMask = (FS_MOVED_TO | FS_MOVED_FROM | FS_DELETE
				 | FS_CREATE | FS_EVENT_ON_CHILD);

static void au_hfsn_free_mark(struct fsnotify_mark_entry *entry)
{
#if 0
	struct au_hnotify *hn = container_of(entry, struct au_hnotify,
					     hn_entry);
	au_cache_free_hnotify(hn);
#endif
	AuDbg("here\n");
}

static int au_hfsn_alloc(struct au_hinode *hinode)
{
	struct au_hnotify *hn;
	struct super_block *sb;
	struct au_branch *br;
	struct fsnotify_mark_entry *entry;
	aufs_bindex_t bindex;

	hn = hinode->hi_notify;
	sb = hn->hn_aufs_inode->i_sb;
	bindex = au_br_index(sb, hinode->hi_id);
	br = au_sbr(sb, bindex);
	entry = &hn->hn_entry;
	fsnotify_init_mark(entry, au_hfsn_free_mark);
	entry->mask = AuHfsnMask;
	return fsnotify_add_mark(entry, br->br_hfsn_group, hinode->hi_inode);
}

static void au_hfsn_free(struct au_hinode *hinode)
{
	struct au_hnotify *hn;
	struct fsnotify_mark_entry *entry;

	hn = hinode->hi_notify;
	entry = &hn->hn_entry;
	fsnotify_destroy_mark_by_entry(entry);
	fsnotify_put_mark(entry);
}

/* ---------------------------------------------------------------------- */

static void au_hfsn_ctl(struct au_hinode *hinode, int do_set)
{
	struct fsnotify_mark_entry *entry;

	entry = &hinode->hi_notify->hn_entry;
	spin_lock(&entry->lock);
	if (do_set) {
		AuDebugOn(entry->mask & AuHfsnMask);
		entry->mask |= AuHfsnMask;
	} else {
		AuDebugOn(!(entry->mask & AuHfsnMask));
		entry->mask &= ~AuHfsnMask;
	}
	spin_unlock(&entry->lock);
	/* fsnotify_recalc_inode_mask(hinode->hi_inode); */
}

/* ---------------------------------------------------------------------- */

/* #define AuDbgHnotify */
#ifdef AuDbgHnotify
static char *au_hfsn_name(u32 mask)
{
#ifdef CONFIG_AUFS_DEBUG
#define test_ret(flag)	if (mask & flag) \
				return #flag;
	test_ret(FS_ACCESS);
	test_ret(FS_MODIFY);
	test_ret(FS_ATTRIB);
	test_ret(FS_CLOSE_WRITE);
	test_ret(FS_CLOSE_NOWRITE);
	test_ret(FS_OPEN);
	test_ret(FS_MOVED_FROM);
	test_ret(FS_MOVED_TO);
	test_ret(FS_CREATE);
	test_ret(FS_DELETE);
	test_ret(FS_DELETE_SELF);
	test_ret(FS_MOVE_SELF);
	test_ret(FS_UNMOUNT);
	test_ret(FS_Q_OVERFLOW);
	test_ret(FS_IN_IGNORED);
	test_ret(FS_IN_ISDIR);
	test_ret(FS_IN_ONESHOT);
	test_ret(FS_EVENT_ON_CHILD);
	return "";
#undef test_ret
#else
	return "??";
#endif
}
#endif

/* ---------------------------------------------------------------------- */

static int au_hfsn_handle_event(struct fsnotify_group *group,
				struct fsnotify_event *event)
{
	int err;
	struct au_hnotify *hnotify;
	struct inode *h_dir, *h_inode;
	__u32 mask;
	struct fsnotify_mark_entry *entry;
	struct qstr h_child_qstr = {
		.name	= event->file_name,
		.len	= event->name_len
	};

	AuDebugOn(event->data_type != FSNOTIFY_EVENT_INODE);

	err = 0;
	/* if IN_UNMOUNT happens, there must be another bug */
	mask = event->mask;
	AuDebugOn(mask & FS_UNMOUNT);
	if (mask & (IN_IGNORED | IN_UNMOUNT))
		goto out;

	h_dir = event->to_tell;
	h_inode = event->inode;
#ifdef AuDbgHnotify
	au_debug(1);
	if (1 || h_child_qstr.len != sizeof(AUFS_XINO_FNAME) - 1
	    || strncmp(h_child_qstr.name, AUFS_XINO_FNAME, h_child_qstr.len)) {
		AuDbg("i%lu, mask 0x%x %s, hcname %.*s, hi%lu\n",
		      h_dir->i_ino, mask, au_hfsn_name(mask),
		      AuLNPair(&h_child_qstr), h_inode ? h_inode->i_ino : 0);
		/* WARN_ON(1); */
	}
	au_debug(0);
#endif

	spin_lock(&h_dir->i_lock);
	entry = fsnotify_find_mark_entry(group, h_dir);
	spin_unlock(&h_dir->i_lock);
	if (entry) {
		hnotify = container_of(entry, struct au_hnotify, hn_entry);
		err = au_hnotify(h_dir, hnotify, mask, &h_child_qstr, h_inode);
		fsnotify_put_mark(entry);
	}

out:
	return err;
}

/* isn't it waste to ask every registered 'group'? */
/* copied from linux/fs/notify/inotify/inotify_fsnotiry.c */
/* it should be exported to modules */
static bool au_hfsn_should_send_event(struct fsnotify_group *group,
				      struct inode *h_inode, __u32 mask)
{
	struct fsnotify_mark_entry *entry;
	bool send;

	spin_lock(&h_inode->i_lock);
	entry = fsnotify_find_mark_entry(group, h_inode);
	spin_unlock(&h_inode->i_lock);
	if (!entry)
		return false;

	mask = (mask & ~FS_EVENT_ON_CHILD);
	send = (entry->mask & mask);

	/* find took a reference */
	fsnotify_put_mark(entry);

	return send;
}

static struct fsnotify_ops au_hfsn_ops = {
	.should_send_event	= au_hfsn_should_send_event,
	.handle_event		= au_hfsn_handle_event
};

/* ---------------------------------------------------------------------- */

static void au_hfsn_fin_br(struct au_branch *br)
{
	if (br->br_hfsn_group)
		fsnotify_put_group(br->br_hfsn_group);
}

static int au_hfsn_init_br(struct au_branch *br, int perm)
{
	br->br_hfsn_group = NULL;
	br->br_hfsn_ops = au_hfsn_ops;
	return 0;
}

static int au_hfsn_reset_br(unsigned int udba, struct au_branch *br, int perm)
{
	int err;
	unsigned int gn;
	const unsigned int gn_max = 10;

	err = 0;
	if (udba != AuOpt_UDBA_HNOTIFY
	    || !au_br_hnotifyable(perm)) {
		au_hfsn_fin_br(br);
		br->br_hfsn_group = NULL;
		goto out;
	}

	if (br->br_hfsn_group)
		goto out;

	gn = 0;
	for (gn = 0; gn < gn_max; gn++) {
		br->br_hfsn_group = fsnotify_obtain_group(gn, AuHfsnMask,
							  &br->br_hfsn_ops);
		if (br->br_hfsn_group != ERR_PTR(-EEXIST))
			break;
	}

	if (IS_ERR(br->br_hfsn_group)) {
		pr_err("fsnotify_obtain_group() failed %u times\n", gn_max);
		err = PTR_ERR(br->br_hfsn_group);
		br->br_hfsn_group = NULL;
	}

out:
	AuTraceErr(err);
	return err;
}

const struct au_hnotify_op au_hnotify_op = {
	.ctl		= au_hfsn_ctl,
	.alloc		= au_hfsn_alloc,
	.free		= au_hfsn_free,

	.reset_br	= au_hfsn_reset_br,
	.fin_br		= au_hfsn_fin_br,
	.init_br	= au_hfsn_init_br
};
