/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>
#include <zephyr/dt-bindings/clock/imx93_ccm.h>
#include <fsl_clock.h>
#include <errno.h>

#define IMX93_CCM_SRC_NUM 4
#define IMX93_CCM_DIV_MAX 255
#define IMX93_CCM_INVAL_RELATIVE 0xdeadbeef

#define IMX93_CCM_PLL_VCO_LEVEL 0
#define IMX93_CCM_PLL_PFD_LEVEL 1
#define IMX93_CCM_PLL_PFD_DIV2_LEVEL 2

#define IMX93_CCM_PLL_MAX_CFG 1

struct imx93_ccm_pll_config {
	/* VCO-specific configuration */
	fracn_pll_init_t vco_cfg;
	/* PFD-specific configuration */
	fracn_pll_pfd_init_t pfd_cfg;
	/* frequency the configuration yields */
	uint32_t freq;
	/* parent configuration */
	struct imx93_ccm_pll_config *parent;
};

struct imx93_ccm_clock {
	/* clock data */
	struct imx_ccm_clock clk;
	/* index of parent/child in roots/clocks array */
	uint32_t relative_idx;
};

struct imx93_ccm_pll {
	/* clock data */
	struct imx_ccm_clock clk;
	/* offset from the analog base */
	uint32_t offset; /* offset from analog base */
	/* PFD number - only applicable to LEVEL 1 PLLs */
	uint32_t pfd;
	/* PLL's parent */
	struct imx93_ccm_pll *parent;
	/* number of predefined configurations */
	uint32_t config_num;
	/* currently selected configuration */
	struct imx93_ccm_pll_config *set_config;
	/* array of predefined configurations */
	struct imx93_ccm_pll_config fracn_configs[IMX93_CCM_PLL_MAX_CFG];
};

/* because of dependencies between PLLs we need a 3-level scheme
 * which will allow the clock configuration based on dependencies.
 *
 * the PLLs are organized as follows (taking SYSTEM_PLL1 as an example)
 *
 * SYSTEM_PLL1 ----- SYSTEM_PLL1_PFD0 ----------- SYSTEM_PLL1_PFD0_DIV2
 *              |                          |
 *              |                          |_____ SYSTEM_PLL1_PFD0
 *              ---- SYSTEM_PLL1_PFD1 ----------- SYSTEM_PLL1_PFD1_DIV2
 *              |                          |
 *              |                          |_____ SYSTEM_PLL1_PFD1
 *              ---- SYSTEM_PLL1_PFD2 ----------- SYSTEM_PLL1_PFD2_DIV2
 *                                         |
 *                                         |_____ SYSTEM_PLL1_PFD2
 *
 * as you can see there are 3 levels:
 *	1) LEVEL 0: SYSTEM_PLL1
 *          * SYSTEM_PLL1 isn't used as a clock source.
 *          This is enforced by imx93_ccm.h and the array of
 *          plls which don't allow it to be selected.
 *          * the output of these PLLs is the VCO post-divider output.
 *
 *      2) LEVEL 1: SYSTEM_PLL1_PFD0, SYSTEM_PLL1_PFD1, SYSTEM_PLL1_PFD2
 *	    * SYSTEM_PLL1_PFDx are not selectable.
 *          * the outputs of these PLLs are the ones from the PFD blocks.
 *
 *      3) LEVEL 2: SYSTEM_PLL1_PFD2, SYSTEM_PLL1_PFD1_DIV2, SYSTEM_PLL1_PFD2_DIV2
 *          * all of the SYSTEM_PLL1_PFDx_DIV2 clocks are selectable.
 *          * the outputs of these PLLs are the ones from the PFD blocks
 *          divided by 2.
 *
 * in terms of dependencies, clocks from a higher level depend on clocks from
 * lower levels. For example, SYSTEM_PLL1_PFD0_DIV2 (LEVEL 2) depends on
 * SYSTEM_PLL1_PFD0 (LEVEL 1) which depends on SYSTEM_PLL1 (LEVEL 0). As such,
 * to configure a LEVEL 2 PLL you need to go through all of the lower levels.
 * Because of this, if a lower level PLL has already been configured then you're
 * not allowed to change its configuration (enforced by the set_config
 * variable) since this would affect the previously configured clocks.
 *
 * although theoretically you could get away with defining the offset
 * from the analog base in the left-most parent of a PLL, due to how the
 * code is built you need to specify said offset for all clocks. For example,
 * you need to set the offset of 0x1100 to all SYSTEM_PLL1_* clocks.
 *
 * because of the many variables in a PLL's output frequency equation
 * (5 for fractional PLLs, 6 for fractional PLLs with PFDs etc...) the
 * way a PLLs frequency is configured is using pre-defined configurations.
 * Each PLL has an array of such configurations. Whenever a user requests
 * a rate through the DTS, the driver looks through the pre-defined
 * configurations to check which configuration would yield the requested
 * rate and uses the data from that configuration to set the PLL registers.
 * At the moment, the rate needs to exactly match the frequency yielded by
 * the configuration (meaning an error of 0 between the requested and the
 * obtained rates).
 *
 * IMPORTANT: please note that SYSTEM_PLL1_PFDx and SYSTEM_PLL1_PFDx_DIV2
 * are different signals.
 *
 * !!!WARNING!!!: configuring a PLL will put it into bypass mode for the
 * duration of the configuration, hence the PLLs can only be configured
 * through the DTS to reduce its effect on various subsystems.
 */

/* array of VCO PLLs. The clock signal is the VCO post-divider frequency */
static struct imx93_ccm_pll pll_vcos[] = {
	/* SYSTEM PLL1 post-divider VCO output */
	{
		.clk.id = kCLOCK_SysPll1,
		.offset = 0x1100,
		.fracn_configs = {
			{
				.vco_cfg.rdiv = 1,
				.vco_cfg.mfi = 166,
				.vco_cfg.mfn = 2,
				.vco_cfg.mfd = 3,
				.vco_cfg.odiv = 4,
				.freq = 4000000000,
			},
		},
		.config_num = 1,
	},
	/* AUDIO_PLL post-divider VCO output */
	{
		.clk.id = kCLOCK_AudioPll1Out,
		.offset = 0x1200,
		.fracn_configs = {
			{
				.vco_cfg.rdiv = 1,
				.vco_cfg.mfi = 81,
				.vco_cfg.mfn = 92,
				.vco_cfg.mfd = 100,
				.vco_cfg.odiv = 5,
				.freq = 393216000,
			},
		},
		.config_num = 1,
	},
};

/* array of PFD PLLs. The clock signal is the PFD block output frequency */
static struct imx93_ccm_pll pll_pfds[] = {
	/* SYSTEM PLL1 PFD0 output */
	{
		.clk.id = kCLOCK_SysPll1Pfd0,
		.parent = &pll_vcos[0],
		.pfd = 0,
		.offset = 0x1100,
		.fracn_configs = {
			{
				.pfd_cfg.mfi = 4,
				.pfd_cfg.mfn = 0,
				.freq = 1000000000,
				.parent = &pll_vcos[0].fracn_configs[0],
			},
		},
		.config_num = 1,
	},
	/* SYSTEM PLL1 PFD1 output */
	{
		.clk.id = kCLOCK_SysPll1Pfd1,
		.parent = &pll_vcos[0],
		.pfd = 1,
		.offset = 0x1100,
		.fracn_configs = {
			{
				.pfd_cfg.mfi = 5,
				.pfd_cfg.mfn = 0,
				.freq = 800000000,
				.parent = &pll_vcos[0].fracn_configs[0],
			},
		},
		.config_num = 1,
	},
};

/* array of PFD DIV2 PLLs. The clock signal is the PFD block output frequency
 * divided by 2.
 */
static struct imx93_ccm_pll pll_pfds_div2[] = {
	/* SYSTEM_PLL1 PFD0 divided by 2 output */
	{
		.clk.id = kCLOCK_SysPll1Pfd0Div2,
		.parent = &pll_pfds[0],
		.offset = 0x1100,
		.fracn_configs = {
			{
				.freq = 500000000,
				.parent = &pll_pfds[0].fracn_configs[0],
			},
		},
		.config_num = 1,
	},
	/* SYSTEM_PLL1 PFD1 divided by 2 output */
	{
		.clk.id = kCLOCK_SysPll1Pfd1Div2,
		.parent = &pll_pfds[1],
		.offset = 0x1100,
		.fracn_configs = {
			{
				.freq = 400000000,
				.parent = &pll_pfds[1].fracn_configs[0],
			},
		},
		.config_num = 1,
	},
};

/* list of PLLs used as clock sources */
static struct imx93_ccm_pll *plls[] = {
	&pll_pfds_div2[0], &pll_pfds_div2[1], &pll_vcos[1],
};

static struct imx_ccm_clock fixed[] = {
	/* 24MHz XTAL */
	{
		.id = kCLOCK_Osc24M,
		.freq = 24000000,
	},
};

static struct imx_ccm_clock *root_mux[] = {
	/* LPUART1 root clock sources */
	&fixed[0],
	(struct imx_ccm_clock *)&pll_pfds_div2[0],
	(struct imx_ccm_clock *)&pll_pfds_div2[1],
	NULL, /* note: VIDEO_PLL currently not supported */

	/* LPUART1 root clock sources */
	&fixed[0],
	(struct imx_ccm_clock *)&pll_pfds_div2[0],
	(struct imx_ccm_clock *)&pll_pfds_div2[1],
	NULL, /* note: VIDEO_PLL currently not supported */

	/* LPUART8 root clock sources */
	&fixed[0],
	(struct imx_ccm_clock *)&pll_pfds_div2[0],
	(struct imx_ccm_clock *)&pll_pfds_div2[1],
	NULL, /* note: VIDEO_PLL currently not supported */
};

static struct imx93_ccm_clock roots[] = {
	{
		.clk.id = kCLOCK_Root_Lpuart1,
		.relative_idx = 0,
	},
	{
		.clk.id = kCLOCK_Root_Lpuart2,
		.relative_idx = 1,
	},
	{
		.clk.id = kCLOCK_Root_Lpuart8,
		.relative_idx = 2,
	},
};

static struct imx93_ccm_clock clocks[] = {
	{
		.clk.id = kCLOCK_Lpuart1,
		.relative_idx = 0,
	},
	{
		.clk.id = kCLOCK_Lpuart2,
		.relative_idx = 1,
	},
	{
		.clk.id = kCLOCK_Lpuart8,
		.relative_idx = 2,
	},
};

static int imx93_ccm_get_clock(uint32_t clk_id, struct imx_ccm_clock **clk)
{
	uint32_t clk_type, clk_idx;

	clk_idx = clk_id & ~IMX93_CCM_TYPE_MASK;
	clk_type = clk_id & IMX93_CCM_TYPE_MASK;

	switch (clk_type) {
	case IMX93_CCM_TYPE_IP:
		if (clk_idx >= ARRAY_SIZE(clocks)) {
			return -EINVAL;
		}
		*clk = (struct imx_ccm_clock *)&clocks[clk_idx];
		break;
	case IMX93_CCM_TYPE_ROOT:
		if (clk_idx >= ARRAY_SIZE(roots)) {
			return -EINVAL;
		}
		*clk = (struct imx_ccm_clock *)&roots[clk_idx];
		break;
	case IMX93_CCM_TYPE_FIXED:
		if (clk_idx >= ARRAY_SIZE(fixed)) {
			return -EINVAL;
		}
		*clk = &fixed[clk_idx];
		break;
	case IMX93_CCM_TYPE_INT_PLL:
	case IMX93_CCM_TYPE_FRACN_PLL:
		if (clk_idx >= ARRAY_SIZE(plls)) {
			return -EINVAL;
		}
		*clk = (struct imx_ccm_clock *)plls[clk_idx];
		break;
	default:
		return -EINVAL;
	};

	return 0;
}

static struct imx93_ccm_pll_config *get_pll_cfg(struct imx93_ccm_pll *pll,
						      uint32_t rate)
{
	int i;

	for (i = 0; i < pll->config_num; i++) {
		if (pll->fracn_configs[i].freq == rate)
			return &pll->fracn_configs[i];
	}

	return NULL;
}

static bool imx93_ccm_rate_is_valid(const struct device *dev,
				    uint32_t clk_id, uint32_t rate)
{
	struct imx_ccm_data *data;
	uint32_t clk_type, clk_idx, mux;
	struct imx_ccm_clock *src, *clk;
	struct imx93_ccm_clock *imx93_clk;
	int ret;

	clk_idx = clk_id & ~IMX93_CCM_TYPE_MASK;
	clk_type = clk_id & IMX93_CCM_TYPE_MASK;
	data = dev->data;

	/* can't set a clock's frequency to 0 */
	if (!rate) {
		return false;
	}

	ret = imx93_ccm_get_clock(clk_id, &clk);
	if (ret < 0) {
		return false;
	}

	switch (clk_type) {
	case IMX93_CCM_TYPE_IP:
		imx93_clk = (struct imx93_ccm_clock *)clk;

		if (imx93_clk->relative_idx >= ARRAY_SIZE(roots)) {
			return -EINVAL;
		}

		clk = (struct imx_ccm_clock *)&roots[imx93_clk->relative_idx];
	case IMX93_CCM_TYPE_ROOT:
		mux = CLOCK_GetRootClockMux(clk->id);
		src = root_mux[clk_idx * IMX93_CCM_SRC_NUM + mux];
		if (!src) {
			return -EINVAL;
		}

		return rate <= src->freq;
	case IMX93_CCM_TYPE_FIXED:
		/* you're not allowed to set a fixed clock's frequency */
		return false;
	case IMX93_CCM_TYPE_INT_PLL:
	case IMX93_CCM_TYPE_FRACN_PLL:
		if (clk_idx >= ARRAY_SIZE(plls)) {
			return false;
		}

		return get_pll_cfg(plls[clk_idx], rate) ? true : false;
	default:
		return false;
	}

	return true;
}

static int imx93_ccm_on_off(const struct device *dev, uint32_t clk_id, bool on)
{
	struct imx_ccm_data *data;
	struct imx_ccm_clock *clk;
	uint32_t type;
	int ret;

	data = dev->data;
	type = clk_id & IMX93_CCM_TYPE_MASK;

	ret = imx93_ccm_get_clock(clk_id, &clk);
	if (ret < 0) {
		return ret;
	}

	switch (type) {
	case IMX93_CCM_TYPE_IP:
		if (on) {
			CLOCK_EnableClock(clk->id);
		} else {
			CLOCK_DisableClock(clk->id);
		}
		break;
	/* gating is not applicable to the following clocks */
	case IMX93_CCM_TYPE_INT_PLL:
	case IMX93_CCM_TYPE_FRACN_PLL:
	case IMX93_CCM_TYPE_ROOT:
	case IMX93_CCM_TYPE_FIXED:
	default:
		return 0;
	}

	return 0;
}


static int imx93_ccm_pll_get_level(struct imx93_ccm_pll *pll)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pll_vcos); i++) {
		if (pll == &pll_vcos[i])
			return IMX93_CCM_PLL_VCO_LEVEL;
	}

	for (i = 0; i < ARRAY_SIZE(pll_pfds); i++) {
		if (pll == &pll_pfds[i])
			return IMX93_CCM_PLL_PFD_LEVEL;
	}

	for (i = 0; i < ARRAY_SIZE(pll_pfds_div2); i++) {
		if (pll == &pll_pfds_div2[i])
			return IMX93_CCM_PLL_PFD_DIV2_LEVEL;
	}

	return -EINVAL;
}

static int imx93_ccm_pll_configure_by_level(const struct device *dev,
					    struct imx93_ccm_pll *pll,
					    uint32_t level,
					    struct imx93_ccm_pll_config *cfg)
{
	struct imx_ccm_data *data;
	fracn_pll_init_t fracn_cfg;
	fracn_pll_pfd_init_t fracn_pfd_cfg;

	data = dev->data;

	/* can't change parent's configuration */
	if (pll->set_config && pll->set_config != cfg) {
		return -EINVAL;
	}

	switch (level) {
	case IMX93_CCM_PLL_VCO_LEVEL:
		memcpy(&fracn_cfg, &cfg->vco_cfg, sizeof(fracn_pll_init_t));

		CLOCK_PllInit((PLL_Type *)(data->pll_regmap + pll->offset),
			      &fracn_cfg);

		break;
	case IMX93_CCM_PLL_PFD_DIV2_LEVEL:
		/* unfortunately, the NXP HAL doesn't support enabling
		 * the DIV2 PFD output. As such, to avoid having to define
		 * LEVEL1 clock data in LEVEL2 clocks, we're going to pull
		 * the required configuration from the parent LEVEL1 clock.
		 */
		__ASSERT(!cfg->parent,
			 "encountered NULL parent configuration for LEVEL 2 PLL");

		memcpy(&fracn_pfd_cfg, &cfg->parent->pfd_cfg,
		       sizeof(fracn_pll_pfd_init_t));

		fracn_pfd_cfg.div2_en = true;

		CLOCK_PllPfdInit((PLL_Type *)(data->pll_regmap + pll->offset),
				 pll->pfd, &fracn_pfd_cfg);
		break;
	case IMX93_CCM_PLL_PFD_LEVEL:
		memcpy(&fracn_pfd_cfg, &cfg->pfd_cfg,
		       sizeof(fracn_pll_pfd_init_t));

		CLOCK_PllPfdInit((PLL_Type *)(data->pll_regmap + pll->offset),
				 pll->pfd, &fracn_pfd_cfg);

		break;
	default:
		return -EINVAL;
	}

	pll->set_config = cfg;
	pll->clk.freq = cfg->freq;

	return cfg->freq;
}

static int imx93_ccm_set_fracn_pll_rate(const struct device *dev,
					struct imx93_ccm_pll *pll,
					struct imx93_ccm_pll_config *cfg)
{
	struct imx93_ccm_pll *pll_parent;
	struct imx93_ccm_pll_config *cfg_parent;
	int pll_level, ret;

	/* what level are we on? */
	pll_level = imx93_ccm_pll_get_level(pll);
	if (pll_level < 0) {
		return -EINVAL;
	}

	/* we can't go any higher in the PLL hierarchy */
	if (!pll->parent) {
		/* we're currently on the last level */
		return imx93_ccm_pll_configure_by_level(dev, pll, pll_level, cfg);
	}

	/* get PLL's parent */
	pll_parent = pll->parent;

	/* get parent configuration */
	cfg_parent = cfg->parent;

	/* need to set configuration for parent */
	ret = imx93_ccm_set_fracn_pll_rate(dev, pll_parent, cfg_parent);
	if (ret < 0) {
		return ret;
	}

	/* set current level's configuration */
	return imx93_ccm_pll_configure_by_level(dev, pll, pll_level, cfg);
}

static int imx93_ccm_set_clock_rate(const struct device *dev, uint32_t clk_id, uint32_t rate)
{
	struct imx_ccm_data *data;
	uint32_t clk_idx, clk_type, div, mux, crt_freq, obtained_freq;
	struct imx_ccm_clock *clk, *root;
	struct imx93_ccm_clock *imx93_clk;
	struct imx93_ccm_pll_config *pll_cfg;
	int ret;

	clk_idx = clk_id & ~IMX93_CCM_TYPE_MASK;
	clk_type = clk_id & IMX93_CCM_TYPE_MASK;
	data = dev->data;

	/* note: this validates the clk index and clk type */
	ret = imx93_ccm_get_clock(clk_id, &clk);
	if (ret < 0) {
		return ret;
	}

	root = clk;

	switch (clk_type) {
	case IMX93_CCM_TYPE_IP:
		imx93_clk = (struct imx93_ccm_clock *)clk;

		if (imx93_clk->relative_idx >= ARRAY_SIZE(roots)) {
			return -EINVAL;
		}

		root = (struct imx_ccm_clock *)&roots[imx93_clk->relative_idx];

		/* is root clock configured? */
		if (!root->freq) {
			return -EINVAL;
		}
	case IMX93_CCM_TYPE_ROOT:
		imx93_clk = (struct imx93_ccm_clock *)root;

		if (imx93_clk->relative_idx == IMX93_CCM_INVAL_RELATIVE) {
			/* this root doesn't have an IP clock child */
			clk = NULL;
		} else {
			if (imx93_clk->relative_idx >= ARRAY_SIZE(clocks)) {
				return -EINVAL;
			}

			clk = (struct imx_ccm_clock
			       *)&clocks[imx93_clk->relative_idx];
		}
		mux = CLOCK_GetRootClockMux(root->id);
		crt_freq = root_mux[clk_idx * IMX93_CCM_SRC_NUM + mux]->freq;

		div = crt_freq / rate;

		if (div > IMX93_CCM_DIV_MAX) {
			return -EINVAL;
		}

		obtained_freq = crt_freq / div;

		CLOCK_SetRootClockDiv(root->id, div);

		if (clk && clk != root) {
			clk->freq = obtained_freq;
		}


		root->freq = obtained_freq;

		/* make sure we also enable the root clock */
		/* TODO: do we need to turn off the root clock before
		 * configuring it?
		 */
		CLOCK_PowerOnRootClock(root->id);

		return obtained_freq;
	case IMX93_CCM_TYPE_FIXED:
		/* can't set a fixed clock's frequency */
		return -EINVAL;
	case IMX93_CCM_TYPE_INT_PLL:
		/* TODO: this needs to be implemented */
		return 0;
	case IMX93_CCM_TYPE_FRACN_PLL:
		pll_cfg = get_pll_cfg((struct imx93_ccm_pll *)clk, rate);
		if (!pll_cfg) {
			return -ENOTSUP;
		}

		return imx93_ccm_set_fracn_pll_rate(dev,
						    (struct imx93_ccm_pll *)clk,
						    pll_cfg);
	default:
		/* this should never be reached */
		return -EINVAL;
	}

	return 0;
}

static int imx93_ccm_assign_parent(const struct device *dev, uint32_t clk_id, uint32_t parent_id)
{
	uint32_t clk_idx, clk_type, parent_idx;
	struct imx_ccm_clock *clk, *parent;
	struct imx_ccm_data *data;
	int ret, i;

	clk_idx = clk_id & ~IMX93_CCM_TYPE_MASK;
	parent_idx = parent_id & ~IMX93_CCM_TYPE_MASK;
	clk_type = clk_id & IMX93_CCM_TYPE_MASK;
	data = dev->data;

	/* fixed clocks don't allow this operation */
	if (clk_type == IMX93_CCM_TYPE_FIXED) {
		return -EINVAL;
	}

	/* IMX93_CCM_DUMMY_CLOCK can be assigned as any clock's parent
	 * except for fixed clocks which shouldn't even be configured
	 * in the first place.
	 */
	if (parent_id == IMX93_CCM_DUMMY_CLOCK) {
		return 0;
	}

	ret = imx93_ccm_get_clock(clk_id, &clk);
	if (ret < 0) {
		return ret;
	}

	ret = imx93_ccm_get_clock(parent_id, &parent);
	if (ret < 0) {
		return ret;
	}

	switch (clk_type) {
	case IMX93_CCM_TYPE_ROOT:
		for (i = 0; i < IMX93_CCM_SRC_NUM; i++) {
			if (root_mux[clk_idx * IMX93_CCM_SRC_NUM + i] == parent) {
				CLOCK_SetRootClockMux(clk->id, i);
				return 0;
			}
		}
		return -EINVAL;
	case IMX93_CCM_TYPE_IP:
		/* note: although IP clocks do in fact have parents
		 * you can't really modify them. As such, it would
		 * be pointless to allow the assign_parent() operation
		 * on an IP clock.
		 */
	case IMX93_CCM_TYPE_INT_PLL:
	case IMX93_CCM_TYPE_FRACN_PLL:
		/* PLLs don't have parents */
		return -EINVAL;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct imx_ccm_clock_api clock_api = {
	.on_off = imx93_ccm_on_off,
	.set_clock_rate = imx93_ccm_set_clock_rate,
	.get_clock = imx93_ccm_get_clock,
	.assign_parent = imx93_ccm_assign_parent,
	.rate_is_valid = imx93_ccm_rate_is_valid,
};

int imx_ccm_init(const struct device *dev)
{
	struct imx_ccm_data *data = dev->data;

	data->api = &clock_api;

	CLOCK_Init((CCM_Type *)data->regmap);

	return 0;
}
