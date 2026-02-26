// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026, Advanced Micro Devices, Inc.
 */

#include <linux/device.h>
#include <linux/iopoll.h>
#include <linux/slab.h>

#include "amdxdna_aie.h"

#define SMU_REG(s, reg) ((s)->smu_regs[reg])

struct smu_device {
	struct device	*dev;
	void __iomem	*smu_regs[SMU_MAX_REGS];
	u32		interval;
	u32		timeout;
};

int aie_smu_exec(struct smu_device *smu, u32 reg_cmd, u32 reg_arg, u32 *out)
{
	u32 resp;
	int ret;

	writel(0, SMU_REG(smu, SMU_RESP_REG));
	writel(reg_arg, SMU_REG(smu, SMU_ARG_REG));
	writel(reg_cmd, SMU_REG(smu, SMU_CMD_REG));

	/* Clear and set SMU_INTR_REG to kick off */
	writel(0, SMU_REG(smu, SMU_INTR_REG));
	writel(1, SMU_REG(smu, SMU_INTR_REG));

	ret = readx_poll_timeout(readl, SMU_REG(smu, SMU_RESP_REG), resp,
				 resp, smu->interval, smu->timeout);
	if (ret) {
		dev_err(smu->dev, "SMU cmd %d timed out", reg_cmd);
		return ret;
	}

	if (out)
		*out = readl(SMU_REG(smu, SMU_OUT_REG));

	if (resp != SMU_RESULT_OK) {
		dev_err(smu->dev, "SMU cmd %d failed, 0x%x", reg_cmd, resp);
		return -EINVAL;
	}

	return 0;
}

struct smu_device *aie_smu_create(struct device *dev, struct smu_config *conf)
{
	struct smu_device *smu;

	smu = devm_kzalloc(dev, sizeof(*smu), GFP_KERNEL);
	if (!smu)
		return NULL;

	smu->dev = dev;
	smu->interval = conf->interval;
	smu->timeout = conf->timeout;
	memcpy(smu->smu_regs, conf->smu_regs, sizeof(smu->smu_regs));

	return smu;
}

