/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>
#include <zephyr/drivers/clock_control.h>
#include <stdlib.h>

#define DT_DRV_COMPAT nxp_imx_ccm_rev3

#define IMX_CCM_REGMAP_IF_EXISTS(nodelabel, idx)		\
	COND_CODE_1(DT_NODE_HAS_PROP(nodelabel, reg),		\
		    (DT_REG_ADDR_BY_IDX(nodelabel, idx)),	\
		    (0))					\

#define IMX_CCM_REGMAP_SIZE_IF_EXISTS(nodelabel, idx)		\
	COND_CODE_1(DT_NODE_HAS_PROP(nodelabel, reg),		\
		    (DT_REG_SIZE_BY_IDX(nodelabel, idx)),	\
		    (0))					\

#define IMX_CCM_MAX_DIV 256

/* these 2 variables need to be exported by every SoC implementing CCM Rev3 */
extern struct imx_ccm_clock_tree clock_tree;
extern struct imx_ccm_clock_api clock_api;

/* note: the clock control API is only allowed to modify/query
 * IP clocks, hence it's not necessary to check the clock's type before
 * performing any operation.
 */

static int imx_ccm_on_off(const struct device *dev,
			  uint32_t clk_idx,
			  bool gate)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_clock *clk;

	cfg = dev->config;

	/* sanity checks */
	if (clk_idx >= cfg->clock_tree->clock_num) {
		return -EINVAL;
	}

	clk = &cfg->clock_tree->clocks[clk_idx];

	if (!cfg->api->gate_ungate) {
		return -EINVAL;
	}

	/* note: the clock will be gated/ungated only if in
	 * uncertain state or in opposite state (gate if clock
	 * is ungated, ungate if clock is gated)
	 *
	 * this way we can avoid unnecessary operations.
	 */
	switch (clk->state) {
	case IMX_CCM_CLOCK_STATE_UNCERTAIN:
		cfg->api->gate_ungate(dev, clk_idx, gate);
		if (gate) {
			clk->state = IMX_CCM_CLOCK_STATE_GATED;
		} else {
			clk->state = IMX_CCM_CLOCK_STATE_UNGATED;
		}
	case IMX_CCM_CLOCK_STATE_GATED:
		if (!gate) {
			cfg->api->gate_ungate(dev, clk_idx, gate);
			clk->state = IMX_CCM_CLOCK_STATE_UNGATED;
		}
	case IMX_CCM_CLOCK_STATE_UNGATED:
		if (gate) {
			cfg->api->gate_ungate(dev, clk_idx, gate);
			clk->state = IMX_CCM_CLOCK_STATE_GATED;
		}
	}

	return 0;
}

static int imx_ccm_on(const struct device *dev, clock_control_subsys_t sys)
{
	return imx_ccm_on_off(dev, (uintptr_t)sys, false);
}

static int imx_ccm_off(const struct device *dev, clock_control_subsys_t sys)
{
	return imx_ccm_on_off(dev, (uintptr_t)sys, true);
}

static int imx_ccm_get_rate(const struct device *dev,
			    clock_control_subsys_t sys, uint32_t *rate)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_clock *clk;
	uint32_t clk_idx;

	cfg = dev->config;
	clk_idx = (uintptr_t)sys;

	/* sanity checks */
	if (clk_idx >= cfg->clock_tree->clock_num) {
		return -EINVAL;
	}

	clk = &cfg->clock_tree->clocks[clk_idx];

	if (!clk->freq) {
		if (!cfg->api->get_rate) {
			return -EINVAL;
		}

		if (cfg->api->get_rate(dev, clk_idx, &clk->freq) < 0) {
			return -EINVAL;
		}
	}

	*rate = clk->freq;

	return 0;
}

static int compute_root_rate(struct imx_ccm_clock *source, uint32_t *rate,
			     uint32_t *div, uint32_t requested_rate)
{
	uint32_t divider;

	/* divide current clock's frequency by the requested
	 * rate. This is done to compute the requested DIV.
	 */
	divider = DIV_ROUND_UP(source->freq, requested_rate);

	if (divider > IMX_CCM_MAX_DIV)
		return -EINVAL;

	/* rate obtained by using computed divider */
	obtained_rate = source->freq / divider;

	/* error - how far off are we from requested rate ?*/
	err = abs(rate - obtained_rate);

	*rate = obtained_rate;
	*div = divider;

	return err;
}

static int imx_ccm_set_rate(const struct device *dev,
			    clock_control_subsys_t sys,
			    clock_control_subsys_rate_t sys_rate)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_clock *clk, *root, **sources;
	uint32_t clk_idx, rate, divider, clk_state, min_rate;
	uint32_t obtained_rate, err, min_err, min_mux, min_div;
	int i, ret;

	cfg = dev->config;
	clk_idx = (uintptr_t)sys;
	rate = (uintptr_t)sys_rate;
	min_err = UINT32_MAX;

	/* sanity checks */
	if (clk_idx >= cfg->clock_tree->clock_num) {
		return -EINVAL;
	}

	clk = &cfg->clock_tree->clocks[clk_idx];
	clk_state = clk->state;

	/* clock already set to requested rate */
	if (clk->freq == rate) {
		return -EALREADY;
	}

	/* can't request a rate of 0 */
	if (!rate) {
		return -EINVAL;
	}

	root = (struct imx_ccm_clock *)clk->parents;
	sources = (struct imx_ccm_clock **)root->parents;

	/* use SoC-specific set_root() function if possible */
	if (cfg->api->set_root) {
		return cfg->api->set_root(dev, root, rate);
	}

	/* at this point, set_root_raw() becomes mandatory */
	if (!cfg->api->set_root_raw) {
		return -EINVAL;
	}

	for (i = 0; i < root->parent_num; i++, sources++) {
		/* TODO: you're not allowed to touch shared root clocks here */

		/* source needs to be configured first */
		if ((*sources)->freq) {
			/* root's frequency is at most equal to the source's */
			if (rate > (*sources)->max_freq) {
				continue;
			}

			/* divide current clock's frequency by the requested
			 * rate. This is done to compute the requested DIV.
			 */
			divider = DIV_ROUND_UP((*sources)->freq, rate);

			if (divider > IMX_CCM_MAX_DIV) {
				continue;
			}

			/* rate obtained by using computed divider */
			obtained_rate = (*sources)->freq / divider;

			/* error - how far off are we from requested rate ?*/
			err = abs(rate - obtained_rate);

			if (err < min_err) {
				min_err = err;
				min_div = divider;
				min_mux = i;
				min_rate = obtained_rate;

				if (!err) {
					break;
				}
			}
		}
	}

	/* could not find a suitable configuration for requested rate */
	if (min_err == UINT32_MAX) {
		return -ENOTSUP;
	}

	/* gate the clock */
	imx_ccm_on_off(dev, clk_idx, true);

	ret = cfg->api->set_root_raw(dev, root, min_mux, min_div);
	if (ret < 0) {
		return ret;
	}

	/* ungate the clock if initially ungated */
	if (clk_state == IMX_CCM_CLOCK_STATE_UNGATED) {
		imx_ccm_on_off(dev, clk_idx, false);
	}

	/* update clock's frequency */
	clk->freq = min_rate;

	return min_rate;
}

static int imx_ccm_init(const struct device *dev)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_data *data;

	cfg = dev->config;
	data = dev->data;

	if (cfg->regmap_phys) {
		device_map(&data->regmap, cfg->regmap_phys,
			   cfg->regmap_size, K_MEM_CACHE_NONE);
	}

	if (cfg->pll_regmap_phys) {
		device_map(&data->pll_regmap, cfg->pll_regmap_phys,
			   cfg->pll_regmap_size, K_MEM_CACHE_NONE);
	}

	if (cfg->api->init) {
		return cfg->api->init(dev);
	}

	return 0;
}

static const struct clock_control_driver_api imx_ccm_api = {
	.on = imx_ccm_on,
	.off = imx_ccm_off,
	.get_rate = imx_ccm_get_rate,
	.set_rate = imx_ccm_set_rate,
};

static struct imx_ccm_data imx_ccm_data;

static struct imx_ccm_config imx_ccm_config = {
	.regmap_phys = IMX_CCM_REGMAP_IF_EXISTS(DT_NODELABEL(ccm_dummy), 0),
	.pll_regmap_phys = IMX_CCM_REGMAP_IF_EXISTS(DT_NODELABEL(ccm_dummy), 1),

	.regmap_size = IMX_CCM_REGMAP_SIZE_IF_EXISTS(DT_NODELABEL(ccm_dummy), 0),
	.pll_regmap_size = IMX_CCM_REGMAP_SIZE_IF_EXISTS(DT_NODELABEL(ccm_dummy), 1),

	.api = &clock_api,
	.clock_tree = &clock_tree,
};

/* there's only 1 CCM instance per SoC */
DEVICE_DT_INST_DEFINE(0,
		      &imx_ccm_init,
		      NULL,
		      &imx_ccm_data, &imx_ccm_config,
		      PRE_KERNEL_1, CONFIG_CLOCK_CONTROL_INIT_PRIORITY,
		      &imx_ccm_api);
