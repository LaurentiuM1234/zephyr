/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>

/* Used for driver binding */
#define DT_DRV_COMPAT nxp_imx_ccm_rev3

extern struct imx_ccm_data imx_ccm_data;
extern struct imx_ccm_config imx_ccm_config;

static int mcux_ccm_on_off(const struct device *dev,
		clock_control_subsys_t sys, bool on)
{
	uint32_t clock_name;
	const struct imx_ccm_config *cfg;
	int i;

	clock_name = (uintptr_t)sys;
	cfg = dev->config;

	for (i = 0; i < cfg->clock_config->clock_num; i++) {
		if (cfg->clock_config->clocks[i].id == clock_name)
			return imx_ccm_clock_on_off(cfg->clock_config->clocks[i], on);
	}

	return -EINVAL;
}

static int mcux_ccm_on(const struct device *dev, clock_control_subsys_t sys)
{
	return mcux_ccm_on_off(dev, sys, true);
}

static int mcux_ccm_off(const struct device *dev, clock_control_subsys_t sys)
{
	return mcux_ccm_on_off(dev, sys, false);
}

static int mcux_ccm_get_rate(const struct device *dev,
		clock_control_subsys_t sys, uint32_t *rate)
{
	uint32_t clock_name;
	const struct imx_ccm_config *cfg;
	int i;

	clock_name = (uintptr_t)sys;
	cfg = dev->config;

	for (i = 0; i < cfg->clock_config->clock_num; i++) {
		if (cfg->clock_config->clocks[i].id == clock_name) {
			*rate = imx_ccm_clock_get_rate(cfg->clock_config->clocks[i]);
			return 0;
		}
	}

	return -EINVAL;
}

static int mcux_ccm_set_rate(const struct device *dev,
		clock_control_subsys_t sys, clock_control_subsys_rate_t rate)
{
	uint32_t clock_name, requested_rate;
	const struct imx_ccm_config *cfg;
	int i;

	clock_name = (uintptr_t)sys;
	requested_rate = (uintptr_t)rate;
	cfg = dev->config;

	for (i = 0; i < cfg->clock_config->clock_num; i++) {
		if (cfg->clock_config->clocks[i].id == clock_name)
			return imx_ccm_clock_set_rate(cfg->clock_config->clocks[i],
					requested_rate);
	}

	return -EINVAL;
}

static int mcux_ccm_init(const struct device *dev)
{
	return imx_ccm_init(dev);
}

static const struct clock_control_driver_api mcux_ccm_driver_api = {
	.on = mcux_ccm_on,
	.off = mcux_ccm_off,
	.get_rate = mcux_ccm_get_rate,
	.set_rate = mcux_ccm_set_rate,
};

/* there's only one CCM per SoC */
DEVICE_DT_INST_DEFINE(0,
		&mcux_ccm_init,
		NULL,
		&imx_ccm_data, &imx_ccm_config,
		PRE_KERNEL_1, CONFIG_CLOCK_CONTROL_INIT_PRIORITY,
		&mcux_ccm_driver_api);
