/*
 * amlogic atv demod driver
 *
 * Author: nengwen.chen <nengwen.chen@amlogic.com>
 *
 *
 * Copyright (C) 2018 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ATV_DEMOD_DEBUG_H__
#define __ATV_DEMOD_DEBUG_H__

extern unsigned int atvdemod_debug_en;

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#undef pr_info
#define pr_info(fmt, ...)\
	do {\
		if (1)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)

#undef pr_dbg
#define pr_dbg(fmt, ...)\
	do {\
		if (atvdemod_debug_en & 0x01)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)

#undef pr_err
#define pr_err(fmt, ...)\
	do {\
		if (1)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)

#undef pr_afc
#define pr_afc(fmt, ...)\
	do {\
		if (atvdemod_debug_en & 0x02)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)

#undef pr_warn
#define pr_warn(fmt, ...)\
	do {\
		if (1)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)

#undef pr_audio
#define pr_audio(fmt, ...)\
	do {\
		if (atvdemod_debug_en & 0x04)\
			printk(fmt, ##__VA_ARGS__);\
	} while (0)


#if defined(CONFIG_DEBUG_FS)
#define AML_ATVDEMOD_DEBUGFS
#endif

#if defined(AML_ATVDEMOD_DEBUGFS)

#if 0 /* Want to add this for debufs params check.*/
#define param_check(name, p, type)\
	{\
		static inline type __always_unused *__check_##name(void)\
		{\
			return p;\
		}	\
	}
#endif

#define DEBUGFS_CREATE_NODE(name, mode, parent, type)\
	{\
		extern type name;\
		debugfs_create_##type(#name, (S_IFREG | (mode)),\
			parent, &(name));\
	}

struct dentry_value {
	struct dentry *dentry;
	int *value;
};

#define DEBUGFS_DENTRY_DEFINE(name)\
	extern int name;\
	struct dentry_value dentry_##name = {\
			.dentry = NULL,\
			.value = &(name)\
		}

#define DEBUGFS_DENTRY_VALUE(name)	(&(dentry_##name))

#define DEBUGFS_CREATE_FILE(name, mode, parent, fops, type)\
	{\
		extern type name;\
		extern struct dentry_value dentry_##name;\
		dentry_##name.dentry = debugfs_create_file(#name,\
			mode, parent, ((void *)&(name)), &(fops));\
	}

#else

#define DEBUGFS_CREATE_NODE(name, mode, parent, type)\
	extern type name;\
	module_param(name, type, mode);\
	MODULE_PARM_DESC(name, "\n" #name "\n")

#define DEBUGFS_CREATE_FILE(name, mode, parent, fops, type)\
	DEBUGFS_CREATE_NODE(name, mode, parent, type)

#endif

extern int aml_atvdemod_create_debugfs(const char *name);
extern void aml_atvdemod_remove_debugfs(void);

#endif /* __ATV_DEMOD_DEBUG_H__ */
