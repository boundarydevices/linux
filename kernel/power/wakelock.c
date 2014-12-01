/*
 * kernel/power/wakelock.c
 *
 * User space wakeup sources support.
 *
 * Copyright (C) 2012 Rafael J. Wysocki <rjw@sisk.pl>
 *
 * This code is based on the analogous interface allowing user space to
 * manipulate wakelocks on Android.
 */

#include <linux/capability.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/wakelock.h>

static DEFINE_MUTEX(wakelocks_lock);

struct wakelock {
	char			*name;
	struct rb_node		node;
	struct wakeup_source	ws;
#ifdef CONFIG_PM_WAKELOCKS_GC
	struct list_head	lru;
#endif
};

static struct rb_root wakelocks_tree = RB_ROOT;

ssize_t pm_show_wakelocks(char *buf, bool show_active)
{
	struct rb_node *node;
	struct wakelock *wl;
	char *str = buf;
	char *end = buf + PAGE_SIZE;

	mutex_lock(&wakelocks_lock);

	for (node = rb_first(&wakelocks_tree); node; node = rb_next(node)) {
		wl = rb_entry(node, struct wakelock, node);
		if (wl->ws.active == show_active)
			str += scnprintf(str, end - str, "%s ", wl->name);
	}
	if (str > buf)
		str--;

	str += scnprintf(str, end - str, "\n");

	mutex_unlock(&wakelocks_lock);
	return (str - buf);
}

#if CONFIG_PM_WAKELOCKS_LIMIT > 0
static unsigned int number_of_wakelocks;

static inline bool wakelocks_limit_exceeded(void)
{
	return number_of_wakelocks > CONFIG_PM_WAKELOCKS_LIMIT;
}

static inline void increment_wakelocks_number(void)
{
	number_of_wakelocks++;
}

static inline void decrement_wakelocks_number(void)
{
	number_of_wakelocks--;
}
#else /* CONFIG_PM_WAKELOCKS_LIMIT = 0 */
static inline bool wakelocks_limit_exceeded(void) { return false; }
static inline void increment_wakelocks_number(void) {}
static inline void decrement_wakelocks_number(void) {}
#endif /* CONFIG_PM_WAKELOCKS_LIMIT */

#ifdef CONFIG_PM_WAKELOCKS_GC
#define WL_GC_COUNT_MAX	100
#define WL_GC_TIME_SEC	300

static LIST_HEAD(wakelocks_lru_list);
static unsigned int wakelocks_gc_count;

static inline void wakelocks_lru_add(struct wakelock *wl)
{
	list_add(&wl->lru, &wakelocks_lru_list);
}

static inline void wakelocks_lru_most_recent(struct wakelock *wl)
{
	list_move(&wl->lru, &wakelocks_lru_list);
}

static void wakelocks_gc(void)
{
	struct wakelock *wl, *aux;
	ktime_t now;

	if (++wakelocks_gc_count <= WL_GC_COUNT_MAX)
		return;

	now = ktime_get();
	list_for_each_entry_safe_reverse(wl, aux, &wakelocks_lru_list, lru) {
		u64 idle_time_ns;
		bool active;

		spin_lock_irq(&wl->ws.lock);
		idle_time_ns = ktime_to_ns(ktime_sub(now, wl->ws.last_time));
		active = wl->ws.active;
		spin_unlock_irq(&wl->ws.lock);

		if (idle_time_ns < ((u64)WL_GC_TIME_SEC * NSEC_PER_SEC))
			break;

		if (!active) {
			wakeup_source_remove(&wl->ws);
			rb_erase(&wl->node, &wakelocks_tree);
			list_del(&wl->lru);
			kfree(wl->name);
			kfree(wl);
			decrement_wakelocks_number();
		}
	}
	wakelocks_gc_count = 0;
}
#else /* !CONFIG_PM_WAKELOCKS_GC */
static inline void wakelocks_lru_add(struct wakelock *wl) {}
static inline void wakelocks_lru_most_recent(struct wakelock *wl) {}
static inline void wakelocks_gc(void) {}
#endif /* !CONFIG_PM_WAKELOCKS_GC */
static struct wakelock *wakelock_lookup_add(const char *name, size_t len,
					    bool add_if_not_found)
{
	struct rb_node **node = &wakelocks_tree.rb_node;
	struct rb_node *parent = *node;
	struct wakelock *wl;

	while (*node) {
		int diff;

		parent = *node;
		wl = rb_entry(*node, struct wakelock, node);
		diff = strncmp(name, wl->name, len);
		if (diff == 0) {
			if (wl->name[len])
				diff = -1;
			else
				return wl;
		}
		if (diff < 0)
			node = &(*node)->rb_left;
		else
			node = &(*node)->rb_right;
	}
	if (!add_if_not_found)
		return ERR_PTR(-EINVAL);

	if (wakelocks_limit_exceeded())
		return ERR_PTR(-ENOSPC);

	/* Not found, we have to add a new one. */
	wl = kzalloc(sizeof(*wl), GFP_KERNEL);
	if (!wl)
		return ERR_PTR(-ENOMEM);

	wl->name = kstrndup(name, len, GFP_KERNEL);
	if (!wl->name) {
		kfree(wl);
		return ERR_PTR(-ENOMEM);
	}
	wl->ws.name = wl->name;
	wakeup_source_add(&wl->ws);
	rb_link_node(&wl->node, parent, node);
	rb_insert_color(&wl->node, &wakelocks_tree);
	wakelocks_lru_add(wl);
	increment_wakelocks_number();
	return wl;
}

int pm_wake_lock(const char *buf)
{
	const char *str = buf;
	struct wakelock *wl;
	u64 timeout_ns = 0;
	size_t len;
	int ret = 0;
	if (!capable(CAP_BLOCK_SUSPEND))
		return -EPERM;

	while (*str && !isspace(*str))
		str++;

	len = str - buf;
	if (!len)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a valid timeout string appended. */
		ret = kstrtou64(skip_spaces(str), 10, &timeout_ns);
		if (ret)
			return -EINVAL;
	}
	mutex_lock(&wakelocks_lock);

	wl = wakelock_lookup_add(buf, len, true);
	if (IS_ERR(wl)) {
		ret = PTR_ERR(wl);
		goto out;
	}
	if (timeout_ns) {
		u64 timeout_ms = timeout_ns + NSEC_PER_MSEC - 1;

		do_div(timeout_ms, NSEC_PER_MSEC);
		__pm_wakeup_event(&wl->ws, timeout_ms);
	} else {
		__pm_stay_awake(&wl->ws);
	}

	wakelocks_lru_most_recent(wl);

 out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

int pm_wake_unlock(const char *buf)
{
	struct wakelock *wl;
	size_t len;
	int ret = 0;

	if (!capable(CAP_BLOCK_SUSPEND))
		return -EPERM;

	len = strlen(buf);
	if (!len)
		return -EINVAL;

	if (buf[len-1] == '\n')
		len--;

	if (!len)
		return -EINVAL;

	mutex_lock(&wakelocks_lock);

	wl = wakelock_lookup_add(buf, len, false);
	if (IS_ERR(wl)) {
		ret = PTR_ERR(wl);
		goto out;
	}
	__pm_relax(&wl->ws);

	wakelocks_lru_most_recent(wl);
	wakelocks_gc();

 out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

#ifdef CONFIG_ANDROID
/* android wakelock */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/syscalls.h>

#define WAKE_LOCK_TYPE_MASK              (0x0f)
#define WAKE_LOCK_INITIALIZED            (1U << 8)
#define WAKE_LOCK_ACTIVE                 (1U << 9)
#define WAKE_LOCK_AUTO_EXPIRE            (1U << 10)
#define WAKE_LOCK_PREVENTING_SUSPEND     (1U << 11)

#define SUSPEND_BACKOFF_THRESHOLD	10
#define SUSPEND_BACKOFF_INTERVAL	10000


enum {
	DEBUG_FAILURE	= BIT(0),
	DEBUG_ERROR	= BIT(1),
	DEBUG_NEW	= BIT(2),
	DEBUG_ACCESS	= BIT(3),
	DEBUG_LOOKUP	= BIT(4),
	DEBUG_SUSPEND   = BIT(5),
	DEBUG_EXIT_SUSPEND = BIT(6),
	DEBUG_WAKE_LOCK    = BIT(7),
	DEBUG_EXPIRE       = BIT(8)
};

static int debug_mask = DEBUG_FAILURE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static DEFINE_SPINLOCK(list_lock);
static LIST_HEAD(inactive_locks);
static struct list_head active_wake_locks[WAKE_LOCK_TYPE_COUNT];
static int current_event_num;
static int suspend_sys_sync_count;
static DEFINE_SPINLOCK(suspend_sys_sync_lock);
static struct workqueue_struct *suspend_sys_sync_work_queue;
static DECLARE_COMPLETION(suspend_sys_sync_comp);
struct workqueue_struct *suspend_work_queue;
struct wake_lock main_wake_lock;
suspend_state_t requested_suspend_state = PM_SUSPEND_MEM;
static struct wake_lock unknown_wakeup;
static struct wake_lock suspend_backoff_lock;
static unsigned suspend_short_count;

static DEFINE_MUTEX(tree_lock);

struct user_wake_lock {
	struct rb_node		node;
	struct wake_lock	wake_lock;
	char			name[0];
};
struct rb_root user_wake_locks;

static struct user_wake_lock *lookup_wake_lock_name(
	const char *buf, int allocate, long *timeoutptr)
{
	struct rb_node **p = &user_wake_locks.rb_node;
	struct rb_node *parent = NULL;
	struct user_wake_lock *l;
	int diff;
	u64 timeout;
	int name_len;
	const char *arg;

	/* Find length of lock name and start of optional timeout string */
	arg = buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg - buf;
	if (!name_len)
		goto bad_arg;
	while (isspace(*arg))
		arg++;

	/* Process timeout string */
	if (timeoutptr && *arg) {
		timeout = simple_strtoull(arg, (char **)&arg, 0);
		while (isspace(*arg))
			arg++;
		if (*arg)
			goto bad_arg;
		/* convert timeout from nanoseconds to jiffies > 0 */
		timeout += (NSEC_PER_SEC / HZ) - 1;
		do_div(timeout, (NSEC_PER_SEC / HZ));
		if (timeout <= 0)
			timeout = 1;
		*timeoutptr = timeout;
	} else if (*arg)
		goto bad_arg;
	else if (timeoutptr)
		*timeoutptr = 0;

	/* Lookup wake lock in rbtree */
	while (*p) {
		parent = *p;
		l = rb_entry(parent, struct user_wake_lock, node);
		diff = strncmp(buf, l->name, name_len);
		if (!diff && l->name[name_len])
			diff = -1;
		if (debug_mask & DEBUG_ERROR)
			pr_info("lookup_wake_lock_name: compare %.*s %s %d\n",
					name_len, buf, l->name, diff);

		if (diff < 0)
			p = &(*p)->rb_left;
		else if (diff > 0)
			p = &(*p)->rb_right;
		else
			return l;
	}

	/* Allocate and add new wakelock to rbtree */
	if (!allocate) {
		if (debug_mask & DEBUG_ERROR)
			pr_info("lookup_wake_lock_name: %.*s not found\n",
				name_len, buf);
		return ERR_PTR(-EINVAL);
	}
	l = kzalloc(sizeof(*l) + name_len + 1, GFP_KERNEL);
	if (l == NULL) {
		if (debug_mask & DEBUG_FAILURE)
			pr_err("lookup_wake_lock_name: failed to allocate "
				"memory for %.*s\n", name_len, buf);
		return ERR_PTR(-ENOMEM);
	}
	memcpy(l->name, buf, name_len);
	if (debug_mask & DEBUG_NEW)
		pr_info("lookup_wake_lock_name: new wake lock %s\n", l->name);
	android_wake_lock_init(&l->wake_lock, WAKE_LOCK_SUSPEND, l->name);
	rb_link_node(&l->node, parent, p);
	rb_insert_color(&l->node, &user_wake_locks);
	return l;

bad_arg:
	if (debug_mask & DEBUG_ERROR)
		pr_info("lookup_wake_lock_name: wake lock, %.*s, bad arg, %s\n",
			name_len, buf, arg);
	return ERR_PTR(-EINVAL);
}

ssize_t wake_lock_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = buf + PAGE_SIZE;
	struct rb_node *n;
	struct user_wake_lock *l;

	mutex_lock(&tree_lock);

	for (n = rb_first(&user_wake_locks); n != NULL; n = rb_next(n)) {
		l = rb_entry(n, struct user_wake_lock, node);
		if (android_wake_lock_active(&l->wake_lock))
			s += scnprintf(s, end - s, "%s ", l->name);
	}
	s += scnprintf(s, end - s, "\n");

	mutex_unlock(&tree_lock);
	return s - buf;
}

ssize_t wake_lock_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	long timeout;
	struct user_wake_lock *l;

	mutex_lock(&tree_lock);
	l = lookup_wake_lock_name(buf, 1, &timeout);
	if (IS_ERR(l)) {
		n = PTR_ERR(l);
		goto bad_name;
	}

	if (debug_mask & DEBUG_ACCESS)
		pr_info("wake_lock_store: %s, timeout %ld\n", l->name, timeout);

	if (timeout)
		android_wake_lock_timeout(&l->wake_lock, timeout);
	else
		android_wake_lock(&l->wake_lock);
bad_name:
	mutex_unlock(&tree_lock);
	return n;
}


ssize_t wake_unlock_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = buf + PAGE_SIZE;
	struct rb_node *n;
	struct user_wake_lock *l;

	mutex_lock(&tree_lock);

	for (n = rb_first(&user_wake_locks); n != NULL; n = rb_next(n)) {
		l = rb_entry(n, struct user_wake_lock, node);
		if (!android_wake_lock_active(&l->wake_lock))
			s += scnprintf(s, end - s, "%s ", l->name);
	}
	s += scnprintf(s, end - s, "\n");

	mutex_unlock(&tree_lock);
	return s - buf;
}

ssize_t wake_unlock_store(
	struct kobject *kobj, struct kobj_attribute *attr,
	const char *buf, size_t n)
{
	struct user_wake_lock *l;

	mutex_lock(&tree_lock);
	l = lookup_wake_lock_name(buf, 0, NULL);
	if (IS_ERR(l)) {
		n = PTR_ERR(l);
		goto not_found;
	}

	if (debug_mask & DEBUG_ACCESS)
		pr_info("wake_unlock_store: %s\n", l->name);

	android_wake_unlock(&l->wake_lock);
not_found:
	mutex_unlock(&tree_lock);
	return n;
}

static void expire_wake_lock(struct wake_lock *lock)
{
	lock->flags &= ~(WAKE_LOCK_ACTIVE | WAKE_LOCK_AUTO_EXPIRE);
	list_del(&lock->link);
	list_add(&lock->link, &inactive_locks);
	if (debug_mask & DEBUG_ACCESS)
		pr_info("expired wake lock %s\n", lock->name);
}

/* Caller must acquire the list_lock spinlock */
static void print_active_locks(int type)
{
	struct wake_lock *lock;
	bool print_expired = true;

	BUG_ON(type >= WAKE_LOCK_TYPE_COUNT);
	list_for_each_entry(lock, &active_wake_locks[type], link) {
		if (lock->flags & WAKE_LOCK_AUTO_EXPIRE) {
			long timeout = lock->expires - jiffies;
			if (timeout > 0)
					pr_info("active wake lock %s, time left %ld\n",
						lock->name, timeout);
			else if (print_expired)
					pr_info("wake lock %s, expired\n", lock->name);
		} else {
			pr_info("active wake lock %s\n", lock->name);
			if (!(debug_mask & DEBUG_ACCESS))
				print_expired = false;
		}
	}
}

static long has_wake_lock_locked(int type)
{
	struct wake_lock *lock, *n;
	long max_timeout = 0;

	BUG_ON(type >= WAKE_LOCK_TYPE_COUNT);
	list_for_each_entry_safe(lock, n, &active_wake_locks[type], link) {
		if (lock->flags & WAKE_LOCK_AUTO_EXPIRE) {
			long timeout = lock->expires - jiffies;
			if (timeout <= 0)
				expire_wake_lock(lock);
			else if (timeout > max_timeout)
				max_timeout = timeout;
		} else
			return -1;
	}
	return max_timeout;
}

static long has_wake_lock(int type)
{
	long ret;
	unsigned long irqflags;
	spin_lock_irqsave(&list_lock, irqflags);
	ret = has_wake_lock_locked(type);
	if (ret && (type == WAKE_LOCK_SUSPEND))
		print_active_locks(type);
	spin_unlock_irqrestore(&list_lock, irqflags);
	return ret;
}

static void suspend_backoff(void)
{
	pr_info("suspend: too many immediate wakeups, back off\n");
	android_wake_lock_timeout(&suspend_backoff_lock,
			  msecs_to_jiffies(SUSPEND_BACKOFF_INTERVAL));
}

static void suspend_sys_sync(struct work_struct *work)
{
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("PM: Syncing filesystems...\n");

	sys_sync();

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("sync done.\n");

	spin_lock(&suspend_sys_sync_lock);
	suspend_sys_sync_count--;
	spin_unlock(&suspend_sys_sync_lock);
}
static DECLARE_WORK(suspend_sys_sync_work, suspend_sys_sync);

void suspend_sys_sync_queue(void)
{
	int ret;

	spin_lock(&suspend_sys_sync_lock);
	ret = queue_work(suspend_sys_sync_work_queue, &suspend_sys_sync_work);
	if (ret)
		suspend_sys_sync_count++;
	spin_unlock(&suspend_sys_sync_lock);
}

static bool suspend_sys_sync_abort;
static void suspend_sys_sync_handler(unsigned long);
static DEFINE_TIMER(suspend_sys_sync_timer, suspend_sys_sync_handler, 0, 0);
/* value should be less then half of input event wake lock timeout value
 * which is currently set to 5*HZ (see drivers/input/evdev.c)
 */
#define SUSPEND_SYS_SYNC_TIMEOUT (HZ/4)
static void suspend_sys_sync_handler(unsigned long arg)
{
	if (suspend_sys_sync_count == 0) {
		complete(&suspend_sys_sync_comp);
	} else if (has_wake_lock(WAKE_LOCK_SUSPEND)) {
		suspend_sys_sync_abort = true;
		complete(&suspend_sys_sync_comp);
	} else {
		mod_timer(&suspend_sys_sync_timer, jiffies +
				SUSPEND_SYS_SYNC_TIMEOUT);
	}
}

int suspend_sys_sync_wait(void)
{
	suspend_sys_sync_abort = false;

	if (suspend_sys_sync_count != 0) {
		mod_timer(&suspend_sys_sync_timer, jiffies +
				SUSPEND_SYS_SYNC_TIMEOUT);
		wait_for_completion(&suspend_sys_sync_comp);
	}
	if (suspend_sys_sync_abort) {
		pr_info("suspend aborted....while waiting for sys_sync\n");
		return -EAGAIN;
	}

	return 0;
}

static void suspend(struct work_struct *work)
{
	int ret;
	int entry_event_num;
	struct timespec ts_entry, ts_exit;

	if (has_wake_lock(WAKE_LOCK_SUSPEND)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("suspend: abort suspend\n");
		return;
	}

	entry_event_num = current_event_num;
	suspend_sys_sync_queue();
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("suspend: enter suspend\n");
	getnstimeofday(&ts_entry);
	ret = pm_suspend(requested_suspend_state);
	getnstimeofday(&ts_exit);

	if (debug_mask & DEBUG_EXIT_SUSPEND) {
		struct rtc_time tm;
		rtc_time_to_tm(ts_exit.tv_sec, &tm);
		pr_info("suspend: exit suspend, ret = %d "
			"(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n", ret,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts_exit.tv_nsec);
	}

	if (ts_exit.tv_sec - ts_entry.tv_sec <= 1) {
		++suspend_short_count;

		if (suspend_short_count == SUSPEND_BACKOFF_THRESHOLD) {
			suspend_backoff();
			suspend_short_count = 0;
		}
	} else {
		suspend_short_count = 0;
	}

	if (current_event_num == entry_event_num) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("suspend: pm_suspend returned with no event\n");
		android_wake_lock_timeout(&unknown_wakeup, HZ / 2);
	}
}

static DECLARE_WORK(suspend_work, suspend);

static void expire_wake_locks(unsigned long data)
{
	long has_lock;
	unsigned long irqflags;

    if (debug_mask & DEBUG_EXPIRE)
		pr_info("expire_wake_locks: start\n");

	spin_lock_irqsave(&list_lock, irqflags);
	print_active_locks(WAKE_LOCK_SUSPEND);
	has_lock = has_wake_lock_locked(WAKE_LOCK_SUSPEND);
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("expire_wake_locks: done, has_lock %ld\n", has_lock);
	if (has_lock == 0)
		queue_work(suspend_work_queue, &suspend_work);
	spin_unlock_irqrestore(&list_lock, irqflags);
}

static DEFINE_TIMER(expire_timer, expire_wake_locks, 0, 0);

void android_wake_lock_init(struct wake_lock *lock, int type, const char *name)
{
	unsigned long irqflags = 0;

	if (name)
		lock->name = name;
	BUG_ON(!lock->name);

	if (debug_mask & DEBUG_WAKE_LOCK)
		pr_info("wake_lock_init name=%s\n", lock->name);

	lock->flags = (type & WAKE_LOCK_TYPE_MASK) | WAKE_LOCK_INITIALIZED;

	INIT_LIST_HEAD(&lock->link);
	spin_lock_irqsave(&list_lock, irqflags);
	list_add(&lock->link, &inactive_locks);
	spin_unlock_irqrestore(&list_lock, irqflags);
}
EXPORT_SYMBOL(android_wake_lock_init);

void android_wake_lock_destroy(struct wake_lock *lock)
{
	unsigned long irqflags;

	if (debug_mask & DEBUG_WAKE_LOCK)
		pr_info("wake_lock_destroy name=%s\n", lock->name);
	spin_lock_irqsave(&list_lock, irqflags);
	lock->flags &= ~WAKE_LOCK_INITIALIZED;
	list_del(&lock->link);
	spin_unlock_irqrestore(&list_lock, irqflags);
}
EXPORT_SYMBOL(android_wake_lock_destroy);

static void android_wake_lock_internal(
	struct wake_lock *lock, long timeout, int has_timeout)
{
	int type;
	unsigned long irqflags;
	long expire_in;

	spin_lock_irqsave(&list_lock, irqflags);
	type = lock->flags & WAKE_LOCK_TYPE_MASK;
	BUG_ON(type >= WAKE_LOCK_TYPE_COUNT);
	BUG_ON(!(lock->flags & WAKE_LOCK_INITIALIZED));

	if (!(lock->flags & WAKE_LOCK_ACTIVE))
		lock->flags |= WAKE_LOCK_ACTIVE;

	list_del(&lock->link);
	if (has_timeout) {
		if (debug_mask & DEBUG_WAKE_LOCK)
			pr_info("wake_lock: %s, type %d, timeout %ld.%03lu\n",
				lock->name, type, timeout / HZ,
				(timeout % HZ) * MSEC_PER_SEC / HZ);
		lock->expires = jiffies + timeout;
		lock->flags |= WAKE_LOCK_AUTO_EXPIRE;
		list_add_tail(&lock->link, &active_wake_locks[type]);
	} else {
		if (debug_mask & DEBUG_WAKE_LOCK)
			pr_info("wake_lock: %s, type %d\n", lock->name, type);
		lock->expires = LONG_MAX;
		lock->flags &= ~WAKE_LOCK_AUTO_EXPIRE;
		list_add(&lock->link, &active_wake_locks[type]);
	}
	if (type == WAKE_LOCK_SUSPEND) {
		current_event_num++;
		if (has_timeout)
			expire_in = has_wake_lock_locked(type);
		else
			expire_in = -1;
		if (expire_in > 0) {
			if (debug_mask & DEBUG_WAKE_LOCK)
				pr_info("wake_lock: %s, start expire timer, "
					"%ld\n", lock->name, expire_in);
			mod_timer(&expire_timer, jiffies + expire_in);
		} else {
			if (del_timer(&expire_timer))
				if (debug_mask & DEBUG_EXPIRE)
					pr_info("wake_lock: %s, stop expire timer\n",
						lock->name);
			if (expire_in == 0)
				queue_work(suspend_work_queue, &suspend_work);
		}
	}
	spin_unlock_irqrestore(&list_lock, irqflags);
}

void android_wake_lock(struct wake_lock *lock)
{
	android_wake_lock_internal(lock, 0, 0);
}
EXPORT_SYMBOL(android_wake_lock);

void android_wake_lock_timeout(struct wake_lock *lock, long timeout)
{
	android_wake_lock_internal(lock, timeout, 1);
}
EXPORT_SYMBOL(android_wake_lock_timeout);

void android_wake_unlock(struct wake_lock *lock)
{
	int type;
	unsigned long irqflags;
	spin_lock_irqsave(&list_lock, irqflags);
	type = lock->flags & WAKE_LOCK_TYPE_MASK;

	if (debug_mask & DEBUG_WAKE_LOCK)
		pr_info("wake_unlock: %s\n", lock->name);

	lock->flags &= ~(WAKE_LOCK_ACTIVE | WAKE_LOCK_AUTO_EXPIRE);
	list_del(&lock->link);
	list_add(&lock->link, &inactive_locks);
	if (type == WAKE_LOCK_SUSPEND) {
		long has_lock = has_wake_lock_locked(type);
		if (has_lock > 0) {
			if (debug_mask & DEBUG_EXPIRE)
				pr_info("wake_unlock: %s, start expire timer, "
					"%ld\n", lock->name, has_lock);
			mod_timer(&expire_timer, jiffies + has_lock);
		} else {
			if (del_timer(&expire_timer))
				if (debug_mask & DEBUG_EXPIRE)
					pr_info("wake_unlock: %s, stop expire "
						"timer\n", lock->name);
			if (has_lock == 0)
				queue_work(suspend_work_queue, &suspend_work);
		}
		if (lock == &main_wake_lock)
				print_active_locks(WAKE_LOCK_SUSPEND);

	}
	spin_unlock_irqrestore(&list_lock, irqflags);
}
EXPORT_SYMBOL(android_wake_unlock);

int android_wake_lock_active(struct wake_lock *lock)
{
	return !!(lock->flags & WAKE_LOCK_ACTIVE);
}
EXPORT_SYMBOL(android_wake_lock_active);

static int power_suspend_late(struct device *dev)
{
	int ret = has_wake_lock(WAKE_LOCK_SUSPEND) ? -EAGAIN : 0;
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("power_suspend_late return %d\n", ret);
	return ret;
}

static const struct dev_pm_ops power_driver_pm_ops = {
	.suspend_noirq = power_suspend_late,
};

static const struct platform_driver power_driver = {
	.driver.name = "power",
	.driver.pm = &power_driver_pm_ops,
};


static int __init wakelocks_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(active_wake_locks); i++)
		INIT_LIST_HEAD(&active_wake_locks[i]);

	android_wake_lock_init(&main_wake_lock, WAKE_LOCK_SUSPEND, "main");
	android_wake_lock(&main_wake_lock);
	android_wake_lock_init(&unknown_wakeup, WAKE_LOCK_SUSPEND, "unknown_wakeups");
	android_wake_lock_init(&suspend_backoff_lock, WAKE_LOCK_SUSPEND,
		       "suspend_backoff");

	ret = platform_driver_register(&power_driver);
	if (ret) {
		pr_err("wakelocks_init: platform_driver_register failed\n");
		goto err_platform_driver_register;
	}

	INIT_COMPLETION(suspend_sys_sync_comp);
	suspend_sys_sync_work_queue =
		create_singlethread_workqueue("suspend_sys_sync");
	if (suspend_sys_sync_work_queue == NULL) {
		ret = -ENOMEM;
		goto err_suspend_sys_sync_work_queue;
	}

	suspend_work_queue = create_singlethread_workqueue("suspend");
	if (suspend_work_queue == NULL) {
		ret = -ENOMEM;
		goto err_suspend_work_queue;
	}

	return 0;

err_suspend_sys_sync_work_queue:
err_suspend_work_queue:
	platform_driver_unregister(&power_driver);
err_platform_driver_register:
	android_wake_lock_destroy(&suspend_backoff_lock);
	android_wake_lock_destroy(&unknown_wakeup);
	android_wake_lock_destroy(&main_wake_lock);
	return ret;
}

static void  __exit wakelocks_exit(void)
{
	destroy_workqueue(suspend_work_queue);
	destroy_workqueue(suspend_sys_sync_work_queue);
	platform_driver_unregister(&power_driver);
	android_wake_lock_destroy(&suspend_backoff_lock);
	android_wake_lock_destroy(&unknown_wakeup);
	android_wake_lock_destroy(&main_wake_lock);
}
core_initcall(wakelocks_init);
module_exit(wakelocks_exit);
#endif
