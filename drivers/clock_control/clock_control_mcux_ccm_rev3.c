/*
 * Copyight 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/dt-bindings/clock/mcux_ccm_rev3.h>
#include <stdio.h>

/* used for driver binding */
#define DT_DRV_COMPAT nxp_imx_ccm_rev3

/* utility macros */
#define IMX_CCM_REGMAP_IF_EXISTS(nodelabel, idx)		\
	COND_CODE_1(DT_NODE_HAS_PROP(nodelabel, reg),		\
		    (DT_REG_ADDR_BY_IDX(nodelabel, idx)),	\
		    (0))
#define IMX_CCM_REGMAP_SIZE_IF_EXISTS(nodelabel, idx)		\
	COND_CODE_1(DT_NODE_HAS_PROP(nodelabel, reg),		\
		    (DT_REG_SIZE_BY_IDX(nodelabel, idx)),	\
		    (0))

#define APPEND_COMA(...) __VA_ARGS__

#define EXTRACT_CLOCK_ARRAY(node_id, prop)\
	APPEND_COMA(DT_FOREACH_PROP_ELEM_SEP(node_id, prop, DT_PROP_BY_IDX, (,)),)

#define IMX_CCM_FOREACH_ASSIGNED_CLOCK(node_id)				\
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, assigned_clocks),		\
		    (EXTRACT_CLOCK_ARRAY(node_id, assigned_clocks)),	\
		    ())							\

#define IMX_CCM_FOREACH_ASSIGNED_PARENT(node_id)				\
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, assigned_clock_parents),		\
		    (EXTRACT_CLOCK_ARRAY(node_id, assigned_clock_parents)),	\
		    ())								\

#define IMX_CCM_FOREACH_ASSIGNED_RATES(node_id)					\
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, assigned_clock_rates),		\
		    (EXTRACT_CLOCK_ARRAY(node_id, assigned_clock_rates)),	\
		    ())								\

#define IMX_CCM_MAX_DIV 256

static int mcux_ccm_clock_init(const struct device *dev);
static int mcux_ccm_pll_init(const struct device *dev);

static int mcux_ccm_on_off(const struct device *dev,
			   uint32_t clk_idx,
			   bool on)
{
	struct imx_ccm_clock *clk;
	int ret;

	ret = imx_ccm_get_clock(dev, clk_idx, &clk);
	if (ret < 0) {
		return -EINVAL;
	}

	if ((on && clk->state == IMX_CCM_CLOCK_STATE_GATED) ||
	    (!on && clk->state == IMX_CCM_CLOCK_STATE_UNGATED)) {
		ret = imx_ccm_on_off(dev, clk_idx, on);
		if (ret < 0) {
			return ret;
		}
	}

	switch (clk->state) {
	case IMX_CCM_CLOCK_STATE_GATED:
		clk->state = IMX_CCM_CLOCK_STATE_UNGATED;
		break;
	case IMX_CCM_CLOCK_STATE_UNGATED:
		clk->state = IMX_CCM_CLOCK_STATE_GATED;
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

static int mcux_ccm_on(const struct device *dev, clock_control_subsys_t sys)
{
	return mcux_ccm_on_off(dev, (uintptr_t)sys, true);
}

static int mcux_ccm_off(const struct device *dev, clock_control_subsys_t sys)
{
	return mcux_ccm_on_off(dev, (uintptr_t)sys, false);
}

static int mcux_ccm_get_rate(const struct device *dev,
			     clock_control_subsys_t sys, uint32_t *rate)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_data *data;
	struct imx_ccm_clock *clk;
	int ret;

	cfg = dev->config;
	data = dev->data;

	ret = imx_ccm_get_clock(dev, (uintptr_t)sys, &clk);
	if (ret < 0) {
		return ret;
	}

	/* clock not configured yet */
	if (!clk->freq) {
		return -EINVAL;
	}

	*rate = clk->freq;

	return 0;
}

static int mcux_ccm_set_rate(const struct device *dev,
			     clock_control_subsys_t sys,
			     clock_control_subsys_rate_t sys_rate)
{
	struct imx_ccm_clock *clk;
	uint32_t clk_idx, clk_rate;
	int ret;

	clk_idx = (uintptr_t)sys;
	clk_rate = (uintptr_t)sys_rate;

	ret = imx_ccm_get_clock(dev, clk_idx, &clk);
	if (ret < 0) {
		return ret;
	}

	/* the clock should be gated before attempting to set its rate */
	if (clk->state != IMX_CCM_CLOCK_STATE_GATED) {
		return -EINVAL;
	}

	/* is the clock's rate already set to requested rate? */
	if (clk_rate == clk->freq) {
		return -EALREADY;
	}

	/* is this a valid rate? */
	if (!imx_ccm_rate_is_valid(dev, clk_idx, clk_rate)) {
		return -ENOTSUP;
	}

	/* set requested rate */
	return imx_ccm_set_clock_rate(dev, clk_idx, clk_rate);
}


static int mcux_ccm_init(const struct device *dev)
{
	const struct imx_ccm_config *cfg;
	struct imx_ccm_data *data;
	int ret;

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

	/* perform SoC-specific initalization */
	ret = imx_ccm_init(dev);
	if (ret < 0) {
		return ret;
	}

	/* initialize PLLs */
	ret = mcux_ccm_pll_init(dev);
	if (ret < 0) {
		return ret;
	}

	/* initialize clocks specified through assigned-clock* properties */
	return mcux_ccm_clock_init(dev);
}

static const struct clock_control_driver_api mcux_ccm_api = {
	.on = mcux_ccm_on,
	.off = mcux_ccm_off,
	.get_rate = mcux_ccm_get_rate,
	.set_rate = mcux_ccm_set_rate,
};

static uint32_t clocks[] = { DT_FOREACH_NODE(IMX_CCM_FOREACH_ASSIGNED_CLOCK) };
static uint32_t parents[] = { DT_FOREACH_NODE(IMX_CCM_FOREACH_ASSIGNED_PARENT) };
static uint32_t rates[] = { DT_FOREACH_NODE(IMX_CCM_FOREACH_ASSIGNED_RATES) };
static uint32_t plls[] = DT_PROP_OR(DT_NODELABEL(ccm_dummy), plls, {});

/* if present, the number of clocks, parents and rates should be equal.
 * If not, we should throw a build error letting the user know the module has
 * been misconfigured.
 */
BUILD_ASSERT(ARRAY_SIZE(clocks) == ARRAY_SIZE(rates),
	     "number of clocks needs to match number of rates");
BUILD_ASSERT(!ARRAY_SIZE(parents) || ARRAY_SIZE(clocks) == ARRAY_SIZE(parents),
	     "number of clocks needs to match number of parents");

/* the plls property has the following structure:
 *
 * plls = <PLL1_ID PLL1_FLAG PLL1_RATE>, ...
 *
 * as such, the plls array needs to be divisible by 3.
 */
BUILD_ASSERT(!(ARRAY_SIZE(plls) % 3), "malformed plls property");

static int mcux_ccm_pll_init(const struct device *dev)
{
	int i, ret;
	uint32_t pll_num, pll_id, pll_flag, pll_rate;
	struct imx_ccm_clock *clk;

	pll_num = ARRAY_SIZE(plls);

	for (i = 0; i < pll_num; i += 3) {
		pll_id = plls[i];
		pll_flag = plls[i + 1];
		pll_rate = plls[i + 2];

		if (pll_flag & IMX_CCM_FLAG_ASSUME_ON) {
			ret = imx_ccm_get_clock(dev, pll_id, &clk);
			if (ret < 0) {
				return ret;
			}

			clk->freq = pll_rate;
			clk->state = IMX_CCM_CLOCK_STATE_UNGATED;
		} else {
			ret = imx_ccm_set_clock_rate(dev, pll_id, pll_rate);
			if (ret < 0) {
				return ret;
			}
		}
	}

	return 0;
}

static int mcux_ccm_clock_init(const struct device *dev)
{
	const struct imx_ccm_config *cfg;
	int i, ret;
	uint32_t clk_idx, parent, rate;
	uint32_t clock_num, parent_num, rate_num;

	cfg = dev->config;
	clock_num = ARRAY_SIZE(clocks);
	parent_num = ARRAY_SIZE(parents);
	rate_num = ARRAY_SIZE(rates);

	for (i = 0; i < clock_num; i++) {
		clk_idx = clocks[i];

		if (parent_num) {
			parent = parents[i];

			/* force the clock into GATED state */
			ret = mcux_ccm_on_off(dev, clk_idx, false);
			if (ret < 0) {
				return ret;
			}

			/* try to assign parent */
			ret = imx_ccm_assign_parent(dev, clk_idx, parent);
			if (ret < 0) {
				return ret;
			}
		}

		/* is this a valid rate? */
		rate = rates[i];
		if (!imx_ccm_rate_is_valid(dev, clk_idx, rate)) {
			return -EINVAL;
		}

		/* force the clock into GATED state
		 *
		 * note: mcux_ccm_on_off won't perform a GATE operation
		 * if the clock has already been gated.
		 */
		ret = mcux_ccm_on_off(dev, clk_idx, false);
		if (ret < 0) {
			return ret;
		}

		/* set clock's rate */
		ret = imx_ccm_set_clock_rate(dev, clk_idx, rate);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}


struct imx_ccm_data mcux_ccm_data;

struct imx_ccm_config mcux_ccm_config = {
	.regmap_phys = IMX_CCM_REGMAP_IF_EXISTS(DT_NODELABEL(ccm_dummy), 0),
	.pll_regmap_phys = IMX_CCM_REGMAP_IF_EXISTS(DT_NODELABEL(ccm_dummy), 1),

	.regmap_size = IMX_CCM_REGMAP_SIZE_IF_EXISTS(DT_NODELABEL(ccm_dummy), 0),
	.pll_regmap_size = IMX_CCM_REGMAP_SIZE_IF_EXISTS(DT_NODELABEL(ccm_dummy), 1),
};

/* there's only 1 CCM instance per SoC */
DEVICE_DT_INST_DEFINE(0,
		      &mcux_ccm_init,
		      NULL,
		      &mcux_ccm_data, &mcux_ccm_config,
		      APPLICATION, CONFIG_CLOCK_CONTROL_INIT_PRIORITY,
		      &mcux_ccm_api);
