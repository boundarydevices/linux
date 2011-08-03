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
 * inotify for the lower directories (deprecated)
 */

#include "aufs.h"

static const __u32 AuHinMask = (IN_MOVE | IN_DELETE | IN_CREATE);
static struct inotify_handle *au_hin_handle;

/* ---------------------------------------------------------------------- */

static int au_hin_alloc(struct au_hinode *hinode)
{
	int err;
	s32 wd;
	struct au_hnotify *hn;
	struct inode *h_inode;
	struct inotify_watch *watch;

	err = -EEXIST;
	h_inode = hinode->hi_inode;
	wd = inotify_find_watch(au_hin_handle, h_inode, &watch);
	if (wd >= 0) {
		put_inotify_watch(watch);
		goto out;
	}

	err = 0;
	hn = hinode->hi_notify;
	inotify_init_watch(&hn->hn_watch);
	wd = inotify_add_watch(au_hin_handle, &hn->hn_watch, h_inode,
			       AuHinMask);
	if (unlikely(wd < 0)) {
		err = wd;
		put_inotify_watch(&hn->hn_watch);
	}

out:
	return err;
}

static void au_hin_free(struct au_hinode *hinode)
{
	int err;
	struct au_hnotify *hn;

	err = 0;
	hn = hinode->hi_notify;
	if (atomic_read(&hn->hn_watch.count))
		err = inotify_rm_watch(au_hin_handle, &hn->hn_watch);
	if (unlikely(err))
		/* it means the watch is already removed */
		pr_warning("failed inotify_rm_watch() %d\n", err);
}

/* ---------------------------------------------------------------------- */

static void au_hin_ctl(struct au_hinode *hinode, int do_set)
{
	struct inode *h_inode;
	struct inotify_watch *watch;

	h_inode = hinode->hi_inode;
	IMustLock(h_inode);

	/* todo: try inotify_find_update_watch()? */
	watch = &hinode->hi_notify->hn_watch;
	mutex_lock(&h_inode->inotify_mutex);
	/* mutex_lock(&watch->ih->mutex); */
	if (do_set) {
		AuDebugOn(watch->mask & AuHinMask);
		watch->mask |= AuHinMask;
	} else {
		AuDebugOn(!(watch->mask & AuHinMask));
		watch->mask &= ~AuHinMask;
	}
	/* mutex_unlock(&watch->ih->mutex); */
	mutex_unlock(&h_inode->inotify_mutex);
}

/* ---------------------------------------------------------------------- */

#ifdef AuDbgHnotify
static char *in_name(u32 mask)
{
#ifdef CONFIG_AUFS_DEBUG
#define test_ret(flag)	if (mask & flag) \
				return #flag;
	test_ret(IN_ACCESS);
	test_ret(IN_MODIFY);
	test_ret(IN_ATTRIB);
	test_ret(IN_CLOSE_WRITE);
	test_ret(IN_CLOSE_NOWRITE);
	test_ret(IN_OPEN);
	test_ret(IN_MOVED_FROM);
	test_ret(IN_MOVED_TO);
	test_ret(IN_CREATE);
	test_ret(IN_DELETE);
	test_ret(IN_DELETE_SELF);
	test_ret(IN_MOVE_SELF);
	test_ret(IN_UNMOUNT);
	test_ret(IN_Q_OVERFLOW);
	test_ret(IN_IGNORED);
	return "";
#undef test_ret
#else
	return "??";
#endif
}
#endif

static u32 au_hin_conv_mask(u32 mask)
{
	u32 conv;

	conv = 0;
#define do_conv(flag)	do {					\
		conv |= (mask & IN_ ## flag) ? FS_ ## flag : 0; \
	} while (0)
	do_conv(ACCESS);
	do_conv(MODIFY);
	do_conv(ATTRIB);
	do_conv(CLOSE_WRITE);
	do_conv(CLOSE_NOWRITE);
	do_conv(OPEN);
	do_conv(MOVED_FROM);
	do_conv(MOVED_TO);
	do_conv(CREATE);
	do_conv(DELETE);
	do_conv(DELETE_SELF);
	do_conv(MOVE_SELF);
	do_conv(UNMOUNT);
	do_conv(Q_OVERFLOW);
#undef do_conv
#define do_conv(flag)	do {						\
		conv |= (mask & IN_ ## flag) ? FS_IN_ ## flag : 0;	\
	} while (0)
	do_conv(IGNORED);
	/* do_conv(ISDIR); */
	/* do_conv(ONESHOT); */
#undef do_conv

	return conv;
}

static void aufs_inotify(struct inotify_watch *watch, u32 wd __maybe_unused,
			 u32 mask, u32 cookie __maybe_unused,
			 const char *h_child_name, struct inode *h_child_inode)
{
	struct au_hnotify *hnotify;
	struct qstr h_child_qstr = {
		.name = h_child_name
	};

	/* if IN_UNMOUNT happens, there must be another bug */
	AuDebugOn(mask & IN_UNMOUNT);
	if (mask & (IN_IGNORED | IN_UNMOUNT)) {
		put_inotify_watch(watch);
		return;
	}

#ifdef AuDbgHnotify
	au_debug(1);
	if (1 || !h_child_name || strcmp(h_child_name, AUFS_XINO_FNAME)) {
		AuDbg("i%lu, wd %d, mask 0x%x %s, cookie 0x%x, hcname %s,"
		      " hi%lu\n",
		      watch->inode->i_ino, wd, mask, in_name(mask), cookie,
		      h_child_name ? h_child_name : "",
		      h_child_inode ? h_child_inode->i_ino : 0);
		WARN_ON(1);
	}
	au_debug(0);
#endif

	if (h_child_name)
		h_child_qstr.len = strlen(h_child_name);
	hnotify = container_of(watch, struct au_hnotify, hn_watch);
	mask = au_hin_conv_mask(mask);
	au_hnotify(watch->inode, hnotify, mask, &h_child_qstr, h_child_inode);
}

static void aufs_inotify_destroy(struct inotify_watch *watch __maybe_unused)
{
	return;
}

static struct inotify_operations aufs_inotify_ops = {
	.handle_event	= aufs_inotify,
	.destroy_watch	= aufs_inotify_destroy
};

/* ---------------------------------------------------------------------- */

static int __init au_hin_init(void)
{
	int err;

	err = 0;
	au_hin_handle = inotify_init(&aufs_inotify_ops);
	if (IS_ERR(au_hin_handle))
		err = PTR_ERR(au_hin_handle);

	AuTraceErr(err);
	return err;
}

static void au_hin_fin(void)
{
	inotify_destroy(au_hin_handle);
}

const struct au_hnotify_op au_hnotify_op = {
	.ctl		= au_hin_ctl,
	.alloc		= au_hin_alloc,
	.free		= au_hin_free,

	.fin		= au_hin_fin,
	.init		= au_hin_init
};
