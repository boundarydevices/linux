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

static DEFINE_MUTEX(gzvm_list_lock);
static LIST_HEAD(gzvm_list);

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
	return 0;
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
	return register_memslot_addr_range(gzvm, memslot);
}

/* gzvm_vm_ioctl() - Ioctl handler of VM FD */
static long gzvm_vm_ioctl(struct file *filp, unsigned int ioctl,
			  unsigned long arg)
{
	long ret;
	void __user *argp = (void __user *)arg;
	struct gzvm *gzvm = filp->private_data;

	switch (ioctl) {
	case GZVM_SET_USER_MEMORY_REGION: {
		struct gzvm_userspace_memory_region userspace_mem;

		if (copy_from_user(&userspace_mem, argp, sizeof(userspace_mem)))
			return -EFAULT;

		ret = gzvm_vm_ioctl_set_memory_region(gzvm, &userspace_mem);
		break;
	}
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static void gzvm_destroy_vm(struct gzvm *gzvm)
{
	pr_debug("VM-%u is going to be destroyed\n", gzvm->vm_id);

	mutex_lock(&gzvm->lock);

	gzvm_arch_destroy_vm(gzvm->vm_id);

	mutex_lock(&gzvm_list_lock);
	list_del(&gzvm->vm_list);
	mutex_unlock(&gzvm_list_lock);

	mutex_unlock(&gzvm->lock);

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
};

static struct gzvm *gzvm_create_vm(struct gzvm_driver *drv, unsigned long vm_type)
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

	gzvm->gzvm_drv = drv;
	gzvm->vm_id = ret;
	gzvm->mm = current->mm;
	mutex_init(&gzvm->lock);

	mutex_lock(&gzvm_list_lock);
	list_add(&gzvm->vm_list, &gzvm_list);
	mutex_unlock(&gzvm_list_lock);

	pr_debug("VM-%u is created\n", gzvm->vm_id);

	return gzvm;
}

/**
 * gzvm_dev_ioctl_create_vm - Create vm fd
 * @vm_type: VM type. Only supports Linux VM now
 * @drv: GenieZone driver info to be stored in struct gzvm for future usage
 *
 * Return: fd of vm, negative if error
 */
int gzvm_dev_ioctl_create_vm(struct gzvm_driver *drv, unsigned long vm_type)
{
	struct gzvm *gzvm;

	gzvm = gzvm_create_vm(drv, vm_type);
	if (IS_ERR(gzvm))
		return PTR_ERR(gzvm);

	return anon_inode_getfd("gzvm-vm", &gzvm_vm_fops, gzvm,
			       O_RDWR | O_CLOEXEC);
}
