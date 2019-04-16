/*
 * NNP-I Linux Driver
 * Copyright (c) 2017-2019, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef _PROJECT_SETTINGS_H_
#define _PROJECT_SETTINGS_H_

#ifdef __KERNEL__
#include <linux/bitops.h>
#else
#include "stdint_ext.h"
#endif
#include "cve_project_internal.h"


enum cve_page_table_flags {
	NEED_TLB_INVALIDATION_IF_PAGES_ADDED = (1 << 0),
	NEED_RESET_DEVICE_IF_PAGES_REMOVED = (1 << 1)
};

struct cve_driver_settings {
	enum cve_page_table_flags flags;
	struct cve_device_groups_config *config;
};

extern struct cve_driver_settings g_driver_settings;

void cve_print_driver_settings(void);

/* Common Device Group Configurations */

/* Default DG configuration used in multi CVEs */
#define DG_CONFIG_SINGLE_GROUP_ALL_DEVICES \
{								\
	1,							\
	12,							\
	{							\
		{12, 8*1024*1024},		\
	}							\
}

/* Configuration that is used by FPGA */
#define DG_CONFIG_SINGLE_GROUP_SINGLE_DEVICE \
{								\
	1,							\
	1,							\
	{							\
		{1, 8*1024*1024},		\
	}							\
}

#endif /* _PROJECT_SETTINGS_H_ */
