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

#define IMX_CCM_FIXED_FREQ(src) ((src).source.fixed.freq)
#define IMX_CCM_PLL_MAX_FREQ(src) ((src).source.pll.max_freq)
#define IMX_CCM_RATE_LIMIT(src) \
	((src).type == IMX_CCM_TYPE_FIXED ? \
	 IMX_CCM_FIXED_FREQ(src) : IMX_CCM_PLL_MAX_FREQ(src))

enum imx_ccm_type {
	IMX_CCM_TYPE_FIXED = 1,
	IMX_CCM_TYPE_PLL,
	IMX_CCM_TYPE_MAX,
};

struct imx_ccm_fixed {
	char *name;
	uint32_t id;
	uint32_t freq;
};

struct imx_ccm_pll {
	char *name;
	uint32_t id;
	uint32_t max_freq; /* maximum allowed frequency */
	uint32_t offset; /* offset from PLL regmap */
};

struct imx_ccm_source {
	enum imx_ccm_type type; /* source type - fixed or PLL */
	union {
		struct imx_ccm_fixed fixed;
		struct imx_ccm_pll pll;
	} source;
};

struct imx_ccm_clock_root {
	char *name;
	uint32_t id;
	struct imx_ccm_source sources[IMX_CCM_MAX_SOURCES];
	uint32_t source_num;
};

struct imx_ccm_clock {
	char *name;
	uint32_t id;
	struct imx_ccm_clock_root root;

	uint32_t lpcg_regmap_phys;
	uint32_t lpcg_regmap_size;
	mm_reg_t lpcg_regmap;
};

struct imx_ccm_clock_config {
	uint32_t clock_num;
	struct imx_ccm_clock *clocks;
};

struct imx_ccm_config {
	struct imx_ccm_clock_config *clock_config;

	uint32_t regmap_phys;
	uint32_t regmap_size;

	uint32_t pll_regmap_phys;
	uint32_t pll_regmap_size;
};


struct imx_ccm_data {
	mm_reg_t regmap;
	mm_reg_t pll_regmap;

	uint32_t ipc_handle;
};

/* disable/enable a given clock
 *
 * it's up to the user of the clock control API to
 * make sure that the sequence of operations is valid.
 */
int imx_ccm_clock_on_off(struct imx_ccm_clock clk, bool on);

/* get the frequency of a given clock */
int imx_ccm_clock_get_rate(struct imx_ccm_clock clk);

/* set the rate of a clock.
 *
 * if successful, the function will return the new rate which
 * may differ from the requested rate.
 */
int imx_ccm_clock_set_rate(struct imx_ccm_clock clk, uint32_t rate);

int imx_ccm_init(const struct device *dev);

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCUX_CCM_H_ */
