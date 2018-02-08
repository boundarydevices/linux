/*
 * include/uapi/drm/meson_drm.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _MESON_DRM_H
#define _MESON_DRM_H

#include <drm/drm.h>

/* memory type definitions. */
enum drm_meson_gem_mem_type {
	/* Physically Continuous memory. */
	MESON_BO_CONTIG	= 1 << 0,
	/* cachable mapping. */
	MESON_BO_CACHABLE	= 1 << 1,
	/* write-combine mapping. */
	MESON_BO_WC		= 1 << 2,
	MESON_BO_SECURE	= 1 << 3,
	MESON_BO_MASK	= MESON_BO_CONTIG | MESON_BO_CACHABLE |
				MESON_BO_WC
};

/* Use flags */
#define BO_USE_NONE			0
#define BO_USE_SCANOUT			(1ull << 0)
#define BO_USE_CURSOR			(1ull << 1)
#define BO_USE_CURSOR_64X64		BO_USE_CURSOR
#define BO_USE_RENDERING		(1ull << 2)
#define BO_USE_LINEAR			(1ull << 3)
#define BO_USE_SW_READ_NEVER		(1ull << 4)
#define BO_USE_SW_READ_RARELY		(1ull << 5)
#define BO_USE_SW_READ_OFTEN		(1ull << 6)
#define BO_USE_SW_WRITE_NEVER		(1ull << 7)
#define BO_USE_SW_WRITE_RARELY		(1ull << 8)
#define BO_USE_SW_WRITE_OFTEN		(1ull << 9)
#define BO_USE_EXTERNAL_DISP		(1ull << 10)
#define BO_USE_PROTECTED		(1ull << 11)
#define BO_USE_HW_VIDEO_ENCODER		(1ull << 12)
#define BO_USE_CAMERA_WRITE		(1ull << 13)
#define BO_USE_CAMERA_READ		(1ull << 14)
#define BO_USE_RENDERSCRIPT		(1ull << 16)
#define BO_USE_TEXTURE			(1ull << 17)


/**
 * User-desired buffer creation information structure.
 *
 * @size: user-desired memory allocation size.
 * @flags: user request for setting memory type or cache attributes.
 * @handle: returned a handle to created gem object.
 *     - this handle will be set by gem module of kernel side.
 */
struct drm_meson_gem_create {
	uint64_t size;
	uint32_t flags;
	uint32_t handle;
};

#define DRM_MESON_GEM_CREATE		0x00

#define DRM_IOCTL_MESON_GEM_CREATE		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_MESON_GEM_CREATE, struct drm_meson_gem_create)

#endif /* _MESON_DRM_H */

