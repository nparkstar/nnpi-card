/********************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/

#include <linux/debugfs.h>

int sphcs_init_maint_interface(void);
void sphcs_release_maint_interface(void);
void sphcs_maint_init_debugfs(struct dentry *parent);
