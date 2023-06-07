/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>
#include <zephyr/drivers/firmware/imx_scu.h>
#include <fsl_clock.h>
#include <errno.h>

#define IMX8QM_LPCG_REGMAP_SIZE 0x10000

static struct imx_ccm_clock clocks[] = {
	{
		.name = "adma_lpuart0_clock",
		.id = kCLOCK_DMA_Lpuart0,
		.lpcg_regmap_phys = DMA__LPCG_LPUART0_BASE,
		.lpcg_regmap_size = IMX8QM_LPCG_REGMAP_SIZE,
	},
	{
		.name = "adma_lpuart1_clock",
		.id = kCLOCK_DMA_Lpuart1,
		.lpcg_regmap_phys = DMA__LPCG_LPUART1_BASE,
		.lpcg_regmap_size = IMX8QM_LPCG_REGMAP_SIZE,
	},
	{
		.name = "adma_lpuart2_clock",
		.id = kCLOCK_DMA_Lpuart2,
		.lpcg_regmap_phys = DMA__LPCG_LPUART2_BASE,
		.lpcg_regmap_size = IMX8QM_LPCG_REGMAP_SIZE,
	},
	{
		.name = "adma_lpuart3_clock",
		.id = kCLOCK_DMA_Lpuart3,
		.lpcg_regmap_phys = DMA__LPCG_LPUART3_BASE,
		.lpcg_regmap_size = IMX8QM_LPCG_REGMAP_SIZE,
	},
};

struct imx_ccm_clock_config imx_ccm_clock_config = {
	.clock_num = ARRAY_SIZE(clocks),
	.clocks = clocks,
};

struct imx_ccm_config imx_ccm_config = {
	.clock_config = &imx_ccm_clock_config,
};

struct imx_ccm_data imx_ccm_data;

int imx_ccm_clock_on_off(struct imx_ccm_clock clk, bool on)
{
	bool ret;

	/* dynamically map LPCG regmap */
	if (!clk.lpcg_regmap)
		device_map(&clk.lpcg_regmap, clk.lpcg_regmap_phys,
				clk.lpcg_regmap_size, K_MEM_CACHE_NONE);

	if (on)
		ret = CLOCK_EnableClock(clk.id);
	else
		ret = CLOCK_DisableClock(clk.id);

	if (!ret)
		return -EINVAL;

	return 0;
}

int imx_ccm_clock_get_rate(struct imx_ccm_clock clk)
{
	if (!CLOCK_GetIpFreq(clk.id))
		return -EINVAL;

	return 0;
}

int imx_ccm_clock_set_rate(struct imx_ccm_clock clk, uint32_t rate)
{
	int returned_rate;

	returned_rate = CLOCK_SetIpFreq(clk.id, rate);

	if (rate == returned_rate)
		return -EALREADY;

	if (!rate)
		return -EINVAL;

	return 0;
}

int imx_ccm_init(const struct device *dev)
{
	const struct device *scu_dev;
	struct imx_ccm_data *data;

	data = dev->data;

	scu_dev = DEVICE_DT_GET(DT_NODELABEL(scu));
	data->ipc_handle = imx_scu_get_ipc_handle(scu_dev);

	CLOCK_Init(data->ipc_handle);

	return 0;
}
