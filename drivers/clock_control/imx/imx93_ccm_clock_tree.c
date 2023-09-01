/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>
#include <fsl_clock.h>

#define IMX93_CCM_SOURCE_COUNT 4
#define IMX93_CCM_ROOT_COUNT 3
#define IMX93_CCM_IP_COUNT 3

static struct imx_ccm_clock sources[IMX93_CCM_SOURCE_COUNT] = {
	/* osc_24m */
	{
		.type = IMX_CCM_CLOCK_TYPE_FIXED,
		.id = kCLOCK_Osc24M,
		.freq = 24000000,
		.max_freq = 24000000,
	},
	/* sys_pll1_pfd0_div2 */
	{
		.type = IMX_CCM_CLOCK_TYPE_FIXED,
		.id = kCLOCK_SysPll1Pfd0Div2,
		.freq = 500000000,
		.max_freq = 500000000,
	},
	/* sys_pll1_pfd1_div2 */
	{
		.type = IMX_CCM_CLOCK_TYPE_FIXED,
		.id = kCLOCK_SysPll1Pfd1Div2,
		.freq = 400000000,
		.max_freq = 400000000,
	},
	/* video_pll1 */
	{
		.type = IMX_CCM_CLOCK_TYPE_FRAC_PLL,
		.id = kCLOCK_VideoPll1Out,
		.offset = 0x1400,
		.max_freq = 594000000,
	},
};

static struct imx_ccm_clock *root_parents[IMX93_CCM_ROOT_COUNT * IMX93_CCM_SOURCE_COUNT] = {
	/* LPUART1 root clock sources */
	&sources[0], &sources[1], &sources[2], &sources[3],
	/* LPUART2 root clock sources */
	&sources[0], &sources[1], &sources[2], &sources[3],
	/* LPI2C8 root clock sources */
	&sources[0], &sources[1], &sources[2], &sources[3],
};

static struct imx_ccm_clock roots[IMX93_CCM_ROOT_COUNT] = {
	{
		.type = IMX_CCM_CLOCK_TYPE_ROOT,
		.id = kCLOCK_Root_Lpuart1,
		.parent_num = 4,
		.parents = &root_parents[0],
	},
	{
		.type = IMX_CCM_CLOCK_TYPE_ROOT,
		.id = kCLOCK_Root_Lpuart2,
		.parent_num = 4,
		.parents = &root_parents[4],
	},
	{
		.type = IMX_CCM_CLOCK_TYPE_ROOT,
		.id = kCLOCK_Root_Lpi2c8,
		.parent_num = 4,
		.parents = &root_parents[8],
	},
};

static struct imx_ccm_clock clocks[IMX93_CCM_IP_COUNT] = {
	{
		.type = IMX_CCM_CLOCK_TYPE_IP,
		.id = kCLOCK_Lpuart1,
		.parent_num = 1,
		.parents = &roots[0],
	},
	{
		.type = IMX_CCM_CLOCK_TYPE_IP,
		.id = kCLOCK_Lpuart2,
		.parent_num = 1,
		.parents = &roots[1],
	},
	{
		.type = IMX_CCM_CLOCK_TYPE_IP,
		.id = kCLOCK_Lpi2c8,
		.parent_num = 1,
		.parents = &roots[2],
	},
};

struct imx_ccm_clock_tree clock_tree = {
	.clocks = clocks,
	.clock_num = ARRAY_SIZE(clocks),
};
