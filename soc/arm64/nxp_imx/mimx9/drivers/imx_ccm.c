/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>
#include <fsl_clock.h>
#include <errno.h>

#define IMX93_CCM_MAX_DIV 256

static const struct imx_ccm_source sources[] = {
	{
		.type = IMX_CCM_TYPE_FIXED,
		.source.fixed.name = "osc_24m",
		.source.fixed.id = kCLOCK_Osc24M,
		.source.fixed.freq = 24000000,
	},

	/* Note: this clock is set to a fixed frequency by ROM code after boot */
	{
		.type = IMX_CCM_TYPE_FIXED,
		.source.fixed.name = "sys_pll1_pfd0_div2",
		.source.fixed.id = kCLOCK_SysPll1Pfd0Div2,
		.source.fixed.freq = 500000000,
	},

	/* Note: this clock is set to a fixed frequency by ROM code after boot */
	{
		.type = IMX_CCM_TYPE_FIXED,
		.source.fixed.name = "sys_pll1_pfd1_div2",
		.source.fixed.id = kCLOCK_SysPll1Pfd1Div2,
		.source.fixed.freq = 400000000,
	},

	{
		.type = IMX_CCM_TYPE_PLL,
		.source.pll.name = "video_pll",
		.source.pll.id = kCLOCK_VideoPll1Out,
		.source.pll.offset = 0x1400,
		.source.pll.max_freq = 594000000,
	},
};

static const struct imx_ccm_clock_root roots[] = {
	{
		.name = "lpuart1_clk_root",
		.id = kCLOCK_Root_Lpuart1,
		.sources = {sources[0], sources[1], sources[2], sources[3] },
		.source_num = 4,
	},

	{
		.name = "lpuart2_clk_root",
		.id = kCLOCK_Root_Lpuart2,
		.sources = {sources[0], sources[1], sources[2], sources[3] },
		.source_num = 4,
	},

	{
		.name = "lpuart3_clk_root",
		.id = kCLOCK_Root_Lpuart3,
		.sources = {sources[0], sources[1], sources[2], sources[3] },
		.source_num = 4,
	},

	{
		.name = "lpuart4_clk_root",
		.id = kCLOCK_Root_Lpuart4,
		.sources = {sources[0], sources[1], sources[2], sources[3] },
		.source_num = 4,
	},

	{
		.name = "lpuart5_clk_root",
		.id = kCLOCK_Root_Lpuart5,
		.sources = {sources[0], sources[1], sources[2], sources[3] },
		.source_num = 4,
	},

	{
		.name = "lpuart6_clk_root",
		.id = kCLOCK_Root_Lpuart6,
		.sources = {sources[0], sources[1], sources[2], sources[3] },
		.source_num = 4,
	},

	{
		.name = "lpuart7_clk_root",
		.id = kCLOCK_Root_Lpuart7,
		.sources = {sources[0], sources[1], sources[2], sources[3] },
		.source_num = 4,
	},

	{
		.name = "lpuart8_clk_root",
		.id = kCLOCK_Root_Lpuart8,
		.sources = {sources[0], sources[1], sources[2], sources[3] },
		.source_num = 4,
	},
};

static struct imx_ccm_clock clocks[] = {
	{
		.name = "lpuart1_clock",
		.id = kCLOCK_Lpuart1,
		.root = roots[0],
	},
};

static struct imx_ccm_clock_config imx_ccm_clock_config = {
	.clock_num = ARRAY_SIZE(clocks),
	.clocks = clocks,
};

struct imx_ccm_data imx_ccm_data;

struct imx_ccm_config imx_ccm_config = {
	.clock_config = &imx_ccm_clock_config,

	.regmap_phys = DT_REG_ADDR_BY_IDX(DT_NODELABEL(ccm), 0),
	.regmap_size = DT_REG_SIZE_BY_IDX(DT_NODELABEL(ccm), 0),

	.pll_regmap_phys = DT_REG_ADDR_BY_IDX(DT_NODELABEL(ccm), 1),
	.pll_regmap_size = DT_REG_SIZE_BY_IDX(DT_NODELABEL(ccm), 1),
};

int imx_ccm_init(const struct device *dev)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_data *data;

	cfg = dev->config;
	data = dev->data;

	/* TODO: make sure HAL uses the mapped addresses */
	device_map(&data->regmap, cfg->regmap_phys,
			cfg->regmap_size, K_MEM_CACHE_NONE);
	device_map(&data->pll_regmap, cfg->pll_regmap_phys,
			cfg->pll_regmap_size, K_MEM_CACHE_NONE);

	return 0;
}

int imx_ccm_clock_on_off(struct imx_ccm_clock clk, bool on)
{
	/* by default, the root clock is enabled and the IP clock is gated */
	/* TODO: is it necessary to also enable/disable root? */
	if (on)
		CLOCK_EnableClock(clk.id);
	else
		CLOCK_DisableClock(clk.id);

	return 0;
}

int imx_ccm_clock_get_rate(struct imx_ccm_clock clk)
{
	uint32_t mux, div;

	mux = CLOCK_GetRootClockMux(clk.root.id);
	div = CLOCK_GetRootClockDiv(clk.root.id);

	/* TODO: add support for PLLs */
	if (clk.root.sources[mux].type == IMX_CCM_TYPE_FIXED)
		return IMX_CCM_FIXED_FREQ(clk.root.sources[mux]);

	return 0;
}

int imx_ccm_clock_set_rate(struct imx_ccm_clock clk, uint32_t rate)
{
	int i;
	uint32_t div, crt_rate;

	if (!rate)
		return -EINVAL;

	/* clock already set to requested rate */
	if (imx_ccm_clock_get_rate(clk) == rate)
		return -EALREADY;

	/* go through each source and try to find a proper div value */
	for (i = 0; i < clk.root.source_num; i++) {
		/* check if requested rate is higher than the rate limit */
		if (rate > IMX_CCM_RATE_LIMIT(clk.root.sources[i]))
			continue;

		/* TODO: add support for PLLs */
		if (clk.root.sources[i].type == IMX_CCM_TYPE_FIXED) {
			crt_rate = IMX_CCM_FIXED_FREQ(clk.root.sources[i]);
			div = DIV_ROUND_UP(crt_rate, rate);

			if (div > IMX93_CCM_MAX_DIV)
				continue;

			/* TODO: is this right? */
			imx_ccm_clock_on_off(clk, false);
			CLOCK_SetRootClockMux(clk.root.id, i - 1);
			CLOCK_SetRootClockDiv(clk.root.id, div);
			imx_ccm_clock_on_off(clk, true);

			return crt_rate / div;
		}
	}

	/* could not find a suitable configuration for requested rate */
	return -ENOTSUP;
}
