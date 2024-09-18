// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/kdev_t.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/gzvm_drv.h>
#include <linux/debugfs.h>
#include <trace/hooks/gzvm.h>
#include "gzvm_common.h"

static DEFINE_MUTEX(gzvm_list_lock);
static LIST_HEAD(gzvm_list);

static struct dentry *gzvm_debugfs_dir;

int gzvm_gfn_to_hva_memslot(struct gzvm_memslot *memslot, u64 gfn,
			    u64 *hva_memslot)
{
	u64 offset;

	if (gfn < memslot->base_gfn)
		return -EINVAL;

	offset = gfn - memslot->base_gfn;
	*hva_memslot = memslot->userspace_addr + offset * PAGE_SIZE;
	return 0;
}

/**
 * gzvm_find_memslot() - Find memslot containing this @gpa
 * @vm: Pointer to struct gzvm
 * @gfn: Guest frame number
 *
 * Return:
 * * >=0		- Index of memslot
 * * -EFAULT		- Not found
 */
int gzvm_find_memslot(struct gzvm *vm, u64 gfn)
{
	int i;

	for (i = 0; i < GZVM_MAX_MEM_REGION; i++) {
		if (vm->memslot[i].npages == 0)
			continue;

		if (gfn >= vm->memslot[i].base_gfn &&
		    gfn < vm->memslot[i].base_gfn + vm->memslot[i].npages)
			return i;
	}

	return -EFAULT;
}

/**
 * register_memslot_addr_range() - Register memory region to GenieZone
 * @gzvm: Pointer to struct gzvm
 * @memslot: Pointer to struct gzvm_memslot
 *
 * Return: 0 for success, negative number for error
 */
static int
register_memslot_addr_range(struct gzvm *gzvm, struct gzvm_memslot *memslot)
{
	struct gzvm_memory_region_ranges *region;
	u32 buf_size = PAGE_SIZE * 2;
	u64 gfn;

	region = alloc_pages_exact(buf_size, GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	region->slot = memslot->slot_id;
	region->total_pages = memslot->npages;
	gfn = memslot->base_gfn;
	region->gpa = PFN_PHYS(gfn);

	if (gzvm_arch_set_memregion(gzvm->vm_id, buf_size,
				    virt_to_phys(region))) {
		pr_err("Failed to register memregion to hypervisor\n");
		free_pages_exact(region, buf_size);
		return -EFAULT;
	}

	free_pages_exact(region, buf_size);

	if (gzvm->mem_alloc_mode == GZVM_DEMAND_PAGING)
		return 0;
	return gzvm_vm_populate_mem_region(gzvm, memslot->slot_id);
}

/**
 * memory_region_pre_check() - Preliminary check for userspace memory region
 * @gzvm: Pointer to struct gzvm.
 * @mem: Input memory region from user.
 *
 * Return: true for check passed, false for invalid input.
 */
static bool
memory_region_pre_check(struct gzvm *gzvm,
			struct gzvm_userspace_memory_region *mem)
{
	if (mem->slot >= GZVM_MAX_MEM_REGION)
		return false;

	if (!PAGE_ALIGNED(mem->guest_phys_addr) ||
	    !PAGE_ALIGNED(mem->memory_size))
		return false;

	if (mem->guest_phys_addr + mem->memory_size < mem->guest_phys_addr)
		return false;

	if ((mem->memory_size >> PAGE_SHIFT) > GZVM_MEM_MAX_NR_PAGES)
		return false;

	return true;
}

/**
 * gzvm_vm_ioctl_set_memory_region() - Set memory region of guest
 * @gzvm: Pointer to struct gzvm.
 * @mem: Input memory region from user.
 *
 * Return: 0 for success, negative number for error
 *
 * -EXIO		- The memslot is out-of-range
 * -EFAULT		- Cannot find corresponding vma
 * -EINVAL		- Region size and VMA size mismatch
 */
static int
gzvm_vm_ioctl_set_memory_region(struct gzvm *gzvm,
				struct gzvm_userspace_memory_region *mem)
{
	int ret;
	struct vm_area_struct *vma;
	struct gzvm_memslot *memslot;
	unsigned long size;

	if (memory_region_pre_check(gzvm, mem) != true)
		return -EINVAL;

	memslot = &gzvm->memslot[mem->slot];

	vma = vma_lookup(gzvm->mm, mem->userspace_addr);
	if (!vma)
		return -EFAULT;

	size = vma->vm_end - vma->vm_start;
	if (size != mem->memory_size)
		return -EINVAL;

	memslot->base_gfn = __phys_to_pfn(mem->guest_phys_addr);
	memslot->npages = size >> PAGE_SHIFT;
	memslot->userspace_addr = mem->userspace_addr;
	memslot->vma = vma;
	memslot->flags = mem->flags;
	memslot->slot_id = mem->slot;

	ret = gzvm_arch_memregion_purpose(gzvm, mem);
	if (ret) {
		pr_err("Failed to config memory region for the specified purpose\n");
		return -EFAULT;
	}
	return register_memslot_addr_range(gzvm, memslot);
}

int gzvm_irqchip_inject_irq(struct gzvm *gzvm, unsigned int vcpu_idx,
			    u32 irq, bool level)
{
	gzvm_vcpu_wakeup_all(gzvm);
	return gzvm_arch_inject_irq(gzvm, vcpu_idx, irq, level);
}

static int gzvm_vm_ioctl_irq_line(struct gzvm *gzvm,
				  struct gzvm_irq_level *irq_level)
{
	u32 irq = irq_level->irq;
	u32 vcpu_idx, vcpu2_idx, irq_num;
	bool level = irq_level->level;

	vcpu_idx = FIELD_GET(GZVM_IRQ_LINE_VCPU, irq);
	vcpu2_idx = FIELD_GET(GZVM_IRQ_LINE_VCPU2, irq) * (GZVM_IRQ_VCPU_MASK + 1);
	irq_num = FIELD_GET(GZVM_IRQ_LINE_NUM, irq);

	return gzvm_irqchip_inject_irq(gzvm, vcpu_idx + vcpu2_idx, irq_num,
				       level);
}

static int gzvm_vm_ioctl_create_device(struct gzvm *gzvm, void __user *argp)
{
	struct gzvm_create_device *gzvm_dev;
	void *dev_data = NULL;
	int ret;

	gzvm_dev = (struct gzvm_create_device *)alloc_pages_exact(PAGE_SIZE,
								  GFP_KERNEL);
	if (!gzvm_dev)
		return -ENOMEM;
	if (copy_from_user(gzvm_dev, argp, sizeof(*gzvm_dev))) {
		ret = -EFAULT;
		goto err_free_dev;
	}

	if (gzvm_dev->attr_addr != 0 && gzvm_dev->attr_size != 0) {
		size_t attr_size = gzvm_dev->attr_size;
		void __user *attr_addr = u64_to_user_ptr(gzvm_dev->attr_addr);

		/* Size of device specific data should not be over a page. */
		if (attr_size > PAGE_SIZE)
			return -EINVAL;

		dev_data = alloc_pages_exact(attr_size, GFP_KERNEL);
		if (!dev_data) {
			ret = -ENOMEM;
			goto err_free_dev;
		}

		if (copy_from_user(dev_data, attr_addr, attr_size)) {
			ret = -EFAULT;
			goto err_free_dev_data;
		}
		gzvm_dev->attr_addr = virt_to_phys(dev_data);
	}

	ret = gzvm_arch_create_device(gzvm->vm_id, gzvm_dev);
err_free_dev_data:
	if (dev_data)
		free_pages_exact(dev_data, 0);
err_free_dev:
	free_pages_exact(gzvm_dev, 0);
	return ret;
}

static int gzvm_vm_ioctl_enable_cap(struct gzvm *gzvm,
				    struct gzvm_enable_cap *cap,
				    void __user *argp)
{
	return gzvm_vm_ioctl_arch_enable_cap(gzvm, cap, argp);
}

/* gzvm_vm_ioctl() - Ioctl handler of VM FD */
static long gzvm_vm_ioctl(struct file *filp, unsigned int ioctl,
			  unsigned long arg)
{
	long ret;
	void __user *argp = (void __user *)arg;
	struct gzvm *gzvm = filp->private_data;

	switch (ioctl) {
	case GZVM_CHECK_EXTENSION: {
		ret = gzvm_dev_ioctl_check_extension(gzvm, arg);
		break;
	}
	case GZVM_CREATE_VCPU: {
		ret = gzvm_vm_ioctl_create_vcpu(gzvm, arg);
		break;
	}
	case GZVM_SET_USER_MEMORY_REGION: {
		struct gzvm_userspace_memory_region userspace_mem;

		if (copy_from_user(&userspace_mem, argp, sizeof(userspace_mem)))
			return -EFAULT;

		ret = gzvm_vm_ioctl_set_memory_region(gzvm, &userspace_mem);
		break;
	}
	case GZVM_IRQ_LINE: {
		struct gzvm_irq_level irq_event;

		if (copy_from_user(&irq_event, argp, sizeof(irq_event))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_vm_ioctl_irq_line(gzvm, &irq_event);
		break;
	}
	case GZVM_CREATE_DEVICE: {
		ret = gzvm_vm_ioctl_create_device(gzvm, argp);
		break;
	}
	case GZVM_IRQFD: {
		struct gzvm_irqfd data;

		if (copy_from_user(&data, argp, sizeof(data))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_irqfd(gzvm, &data);
		break;
	}
	case GZVM_IOEVENTFD: {
		struct gzvm_ioeventfd data;

		if (copy_from_user(&data, argp, sizeof(data))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_ioeventfd(gzvm, &data);
		break;
	}
	case GZVM_ENABLE_CAP: {
		struct gzvm_enable_cap cap;

		if (copy_from_user(&cap, argp, sizeof(cap))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_vm_ioctl_enable_cap(gzvm, &cap, argp);
		break;
	}
	case GZVM_SET_DTB_CONFIG: {
		struct gzvm_dtb_config cfg;

		if (copy_from_user(&cfg, argp, sizeof(cfg))) {
			ret = -EFAULT;
			goto out;
		}
		ret = gzvm_arch_set_dtb_config(gzvm, &cfg);
		break;
	}
	default:
		ret = -ENOTTY;
	}
out:
	return ret;
}

/* Invoker of this function is responsible for locking */
static void gzvm_destroy_all_ppage(struct gzvm *gzvm)
{
	struct gzvm_pinned_page *ppage;
	struct rb_node *node;

	node = rb_first(&gzvm->pinned_pages);
	while (node) {
		ppage = rb_entry(node, struct gzvm_pinned_page, node);
		unpin_user_pages_dirty_lock(&ppage->page, 1, true);
		node = rb_next(node);
		rb_erase(&ppage->node, &gzvm->pinned_pages);
		kfree(ppage);
	}
}

static int gzvm_destroy_vm_debugfs(struct gzvm *vm)
{
	debugfs_remove_recursive(vm->debug_dir);
	return 0;
}

static void gzvm_destroy_vm(struct gzvm *gzvm)
{
	size_t allocated_size;

	pr_debug("VM-%u is going to be destroyed\n", gzvm->vm_id);

	mutex_lock(&gzvm->lock);

	gzvm_vm_irqfd_release(gzvm);
	gzvm_destroy_vcpus(gzvm);
	gzvm_arch_destroy_vm(gzvm->vm_id);

	mutex_lock(&gzvm_list_lock);
	list_del(&gzvm->vm_list);
	mutex_unlock(&gzvm_list_lock);

	if (gzvm->demand_page_buffer) {
		allocated_size = GZVM_BLOCK_BASED_DEMAND_PAGE_SIZE / PAGE_SIZE * sizeof(u64);
		free_pages_exact(gzvm->demand_page_buffer, allocated_size);
	}

	mutex_unlock(&gzvm->lock);

	trace_android_vh_gzvm_destroy_vm_post_process(gzvm);

	/* No need to lock here becauese it's single-threaded execution */
	gzvm_destroy_all_ppage(gzvm);

	gzvm_destroy_vm_debugfs(gzvm);

	kfree(gzvm);
}

static int gzvm_vm_release(struct inode *inode, struct file *filp)
{
	struct gzvm *gzvm = filp->private_data;

	gzvm_destroy_vm(gzvm);
	return 0;
}

static const struct file_operations gzvm_vm_fops = {
	.release        = gzvm_vm_release,
	.unlocked_ioctl = gzvm_vm_ioctl,
	.llseek		= noop_llseek,
};

/**
 * setup_vm_demand_paging - Query hypervisor suitable demand page size and set
 * @vm: gzvm instance for setting up demand page size
 *
 * Return: void
 */
static void setup_vm_demand_paging(struct gzvm *vm)
{
	u32 buf_size = GZVM_BLOCK_BASED_DEMAND_PAGE_SIZE / PAGE_SIZE * sizeof(u64);
	struct gzvm_enable_cap cap = {0};
	void *buffer;
	int ret;

	mutex_init(&vm->demand_paging_lock);
	buffer = alloc_pages_exact(buf_size, GFP_KERNEL);
	if (!buffer) {
		/* Fall back to use default page size for demand paging */
		vm->demand_page_gran = PAGE_SIZE;
		vm->demand_page_buffer = NULL;
		return;
	}

	cap.cap = GZVM_CAP_BLOCK_BASED_DEMAND_PAGING;
	cap.args[0] = GZVM_BLOCK_BASED_DEMAND_PAGE_SIZE;
	cap.args[1] = (__u64)virt_to_phys(buffer);
	/* demand_page_buffer is freed when destroy VM */
	vm->demand_page_buffer = buffer;

	ret = gzvm_vm_ioctl_enable_cap(vm, &cap, NULL);
	if (ret == 0) {
		vm->demand_page_gran = GZVM_BLOCK_BASED_DEMAND_PAGE_SIZE;
		/* freed when destroy vm */
		vm->demand_page_buffer = buffer;
	} else {
		vm->demand_page_gran = PAGE_SIZE;
		vm->demand_page_buffer = NULL;
		free_pages_exact(buffer, buf_size);
	}
}

/**
 * hyp_mem_read() - Get size of hypervisor-allocated memory and stage 2 table
 * @file: Pointer to struct file
 * @buf: User space buffer for storing the return value
 * @len: Size of @buf, in bytes
 * @offset: Pointer to loff_t
 *
 * Return: Size of hypervisor-allocated memory and stage 2 table, in bytes
 */
static ssize_t hyp_mem_read(struct file *file, char __user *buf, size_t len,
			    loff_t *offset)
{
	char tmp_buffer[GZVM_MAX_DEBUGFS_VALUE_SIZE] = {0};
	struct gzvm *vm = file->private_data;
	int ret;

	if (*offset == 0) {
		ret = gzvm_arch_get_statistics(vm);
		if (ret)
			return ret;
		snprintf(tmp_buffer, sizeof(tmp_buffer), "%llu\n",
			 vm->stat.protected_hyp_mem);
		if (copy_to_user(buf, tmp_buffer, sizeof(tmp_buffer)))
			return -EFAULT;
		*offset += sizeof(tmp_buffer);
		return sizeof(tmp_buffer);
	}
	return 0;
}

/**
 * shared_mem_read() - Get size of memory shared between host and guest
 * @file: Pointer to struct file
 * @buf: User space buffer for storing the return value
 * @len: Size of @buf, in bytes
 * @offset: Pointer to loff_t
 *
 * Return: Size of memory shared between host and guest, in bytes
 */
static ssize_t shared_mem_read(struct file *file, char __user *buf, size_t len,
			       loff_t *offset)
{
	char tmp_buffer[GZVM_MAX_DEBUGFS_VALUE_SIZE] = {0};
	struct gzvm *vm = file->private_data;
	int ret;

	if (*offset == 0) {
		ret = gzvm_arch_get_statistics(vm);
		if (ret)
			return ret;
		snprintf(tmp_buffer, sizeof(tmp_buffer), "%llu\n",
			 vm->stat.protected_shared_mem);
		if (copy_to_user(buf, tmp_buffer, sizeof(tmp_buffer)))
			return -EFAULT;
		*offset += sizeof(tmp_buffer);
		return sizeof(tmp_buffer);
	}
	return 0;
}

static const struct file_operations hyp_mem_fops = {
	.open = simple_open,
	.read = hyp_mem_read,
	.llseek = no_llseek,
};

static const struct file_operations shared_mem_fops = {
	.open = simple_open,
	.read = shared_mem_read,
	.llseek = no_llseek,
};

static int gzvm_create_vm_debugfs(struct gzvm *vm)
{
	struct dentry *dent;
	char dir_name[GZVM_MAX_DEBUGFS_DIR_NAME_SIZE];

	if (!gzvm_debugfs_dir)
		return -EFAULT;

	if (vm->debug_dir) {
		pr_warn("VM debugfs directory is duplicated\n");
		return 0;
	}

	snprintf(dir_name, sizeof(dir_name), "%d-%d", task_pid_nr(current), vm->vm_id);

	dent = debugfs_lookup(dir_name, gzvm_debugfs_dir);
	if (dent) {
		pr_warn("Debugfs directory is duplicated\n");
		dput(dent);
		return 0;
	}
	dent = debugfs_create_dir(dir_name, gzvm_debugfs_dir);
	vm->debug_dir = dent;

	debugfs_create_file("protected_shared_mem", 0444, dent, vm, &shared_mem_fops);
	debugfs_create_file("protected_hyp_mem", 0444, dent, vm, &hyp_mem_fops);

	return 0;
}

static int setup_mem_alloc_mode(struct gzvm *vm)
{
	int ret;
	struct gzvm_enable_cap cap = {0};

	cap.cap = GZVM_CAP_ENABLE_DEMAND_PAGING;

	ret = gzvm_vm_ioctl_enable_cap(vm, &cap, NULL);
	if (!ret) {
		vm->mem_alloc_mode = GZVM_DEMAND_PAGING;
		setup_vm_demand_paging(vm);
	} else {
		vm->mem_alloc_mode = GZVM_FULLY_POPULATED;
	}

	return 0;
}

static int enable_idle_support(struct gzvm *vm)
{
	int ret;
	struct gzvm_enable_cap cap = {0};

	cap.cap = GZVM_CAP_ENABLE_IDLE;
	ret = gzvm_vm_ioctl_enable_cap(vm, &cap, NULL);
	if (ret)
		pr_info("Hypervisor doesn't support idle\n");
	return ret;
}

static struct gzvm *gzvm_create_vm(unsigned long vm_type)
{
	int ret;
	struct gzvm *gzvm;

	gzvm = kzalloc(sizeof(*gzvm), GFP_KERNEL);
	if (!gzvm)
		return ERR_PTR(-ENOMEM);

	ret = gzvm_arch_create_vm(vm_type);
	if (ret < 0) {
		kfree(gzvm);
		return ERR_PTR(ret);
	}

	gzvm->vm_id = ret;
	gzvm->mm = current->mm;
	mutex_init(&gzvm->lock);
	mutex_init(&gzvm->mem_lock);
	gzvm->pinned_pages = RB_ROOT;

	ret = gzvm_vm_irqfd_init(gzvm);
	if (ret) {
		pr_err("Failed to initialize irqfd\n");
		kfree(gzvm);
		return ERR_PTR(ret);
	}

	ret = gzvm_init_ioeventfd(gzvm);
	if (ret) {
		pr_err("Failed to initialize ioeventfd\n");
		kfree(gzvm);
		return ERR_PTR(ret);
	}

	setup_mem_alloc_mode(gzvm);

	mutex_lock(&gzvm_list_lock);
	list_add(&gzvm->vm_list, &gzvm_list);
	mutex_unlock(&gzvm_list_lock);

	ret = gzvm_create_vm_debugfs(gzvm);
	if (ret)
		pr_debug("Failed to create debugfs for VM-%u\n", gzvm->vm_id);

	pr_debug("VM-%u is created\n", gzvm->vm_id);

	enable_idle_support(gzvm);

	return gzvm;
}

/**
 * gzvm_dev_ioctl_create_vm - Create vm fd
 * @vm_type: VM type. Only supports Linux VM now.
 *
 * Return: fd of vm, negative if error
 */
int gzvm_dev_ioctl_create_vm(unsigned long vm_type)
{
	struct gzvm *gzvm;

	gzvm = gzvm_create_vm(vm_type);
	if (IS_ERR(gzvm))
		return PTR_ERR(gzvm);

	return anon_inode_getfd("gzvm-vm", &gzvm_vm_fops, gzvm,
			       O_RDWR | O_CLOEXEC);
}

void gzvm_destroy_all_vms(void)
{
	struct gzvm *gzvm, *tmp;

	mutex_lock(&gzvm_list_lock);
	if (list_empty(&gzvm_list))
		goto out;

	list_for_each_entry_safe(gzvm, tmp, &gzvm_list, vm_list)
		gzvm_destroy_vm(gzvm);

out:
	mutex_unlock(&gzvm_list_lock);
}

int gzvm_drv_debug_init(void)
{
	if (!debugfs_initialized())
		return 0;

	if (!gzvm_debugfs_dir && !debugfs_lookup("gzvm", gzvm_debugfs_dir))
		gzvm_debugfs_dir = debugfs_create_dir("gzvm", NULL);

	return 0;
}

void gzvm_drv_debug_exit(void)
{
	if (gzvm_debugfs_dir && debugfs_lookup("gzvm", gzvm_debugfs_dir))
		debugfs_remove_recursive(gzvm_debugfs_dir);
}
