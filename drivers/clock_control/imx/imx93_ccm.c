/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>
#include <fsl_clock.h>
#include <errno.h>

static int imx93_ccm_gate_ungate(const struct device *dev,
				 uint32_t clk_idx,
				 bool gate)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_data *data;
	struct imx_ccm_clock *clk;

	cfg = dev->config;
	data = dev->data;
	clk = &cfg->clock_tree->clocks[clk_idx];

	if (gate)
		CLOCK_DisableClockMapped((uint32_t *)data->regmap, clk->id);
	else
		CLOCK_DisableClockMapped((uint32_t *)data->regmap, clk->id);

	return 0;
}

static int imx93_ccm_set_root_raw(const struct device *dev,
				  struct imx_ccm_clock *root,
				  uint32_t mux, uint32_t div)
{
	struct imx_ccm_data *data = dev->data;

	CLOCK_SetRootClockMuxMapped((uint32_t *)data->regmap, root->id, mux);
	CLOCK_SetRootClockDivMapped((uint32_t *)data->regmap, root->id, div);

	return 0;
}

static int imx93_ccm_get_rate(const struct device *dev, uint32_t clk_idx,
			      uint32_t *rate)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_data *data;
	struct imx_ccm_clock *clk, *root, **sources;
	uint32_t mux, div;

	cfg = dev->config;
	data = dev->data;
	clk = &cfg->clock_tree->clocks[clk_idx];
	root = clk->parents;

	mux = CLOCK_GetRootClockMuxMapped((uint32_t *)data->regmap, root->id);
	div = CLOCK_GetRootClockDivMapped((uint32_t *)data->regmap, root->id);

	sources = (struct imx_ccm_clock **)root->parents + mux;

	/* source not configured */
	if (!(*sources)->freq) {
		return -EAGAIN;
	}

	*rate = (*sources)->freq / div;

	return 0;
}

struct imx_ccm_clock_api clock_api = {
	.gate_ungate = imx93_ccm_gate_ungate,
	.set_root_raw = imx93_ccm_set_root_raw,
	.get_rate = imx93_ccm_get_rate,
};
