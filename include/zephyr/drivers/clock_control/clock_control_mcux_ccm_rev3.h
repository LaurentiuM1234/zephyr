/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCUX_CCM_H_
#define ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCUX_CCM_H_

#include <stdint.h>
#include <zephyr/device.h>

#define IMX_CCM_MAX_SOURCES 4

enum imx_ccm_clock_type {
	IMX_CCM_CLOCK_TYPE_IP = 1, /* IP clock */
	IMX_CCM_CLOCK_TYPE_ROOT, /* root clock */
	IMX_CCM_CLOCK_TYPE_FIXED, /* fixed source clock */
	IMX_CCM_CLOCK_TYPE_INT_PLL, /* integer PLL source clock */
	IMX_CCM_CLOCK_TYPE_FRAC_PLL, /* fractional PLL source clock */
};

enum imx_ccm_clock_state {
	IMX_CCM_CLOCK_STATE_UNCERTAIN = 0,
	IMX_CCM_CLOCK_STATE_GATED,
	IMX_CCM_CLOCK_STATE_UNGATED,
};

struct imx_ccm_clock {
	enum imx_ccm_clock_type type; /* the clock type */
	uint32_t id; /* HAL identifier */
	uint32_t freq; /* nominal frequency */
	uint32_t parent_num; /* number of parents */
	uint32_t max_freq; /* maximum allowed frequency */
	uint32_t offset; /* offset from regmap */
	void *parents; /* clock parents */
	uint32_t state; /* clock state */
};

struct imx_ccm_data {
	mm_reg_t regmap;
	mm_reg_t pll_regmap;
};

struct imx_ccm_clock_tree {
	struct imx_ccm_clock *clocks;
	uint32_t clock_num;
};

struct imx_ccm_config {
	uint32_t regmap_phys;
	uint32_t regmap_size;

	uint32_t pll_regmap_phys;
	uint32_t pll_regmap_size;

	struct imx_ccm_clock_api *api;
	struct imx_ccm_clock_tree *clock_tree;
};

struct imx_ccm_clock_api {
	/* SoC-specific initialization function. Called during driver init */
	int (*init)(const struct device *dev);
	int (*gate_ungate)(const struct device *dev,
			   uint32_t clk_idx, bool gate);
	int (*set_root)(const struct device *dev,
			struct imx_ccm_clock *root, uint32_t rate);
	int (*set_root_raw)(const struct device *dev,
			    struct imx_ccm_clock *root,
			    uint32_t mux, uint32_t div);
	int (*get_rate)(const struct device *dev, uint32_t clk_idx, uint32_t *rate);
};

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCUX_CCM_H_ */
