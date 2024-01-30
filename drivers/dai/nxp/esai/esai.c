/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esai.h"

static int esai_config_set(const struct device *dev,
			   const struct dai_config *dai_cfg,
			   const void *bespoke_data)
{
	const struct esai_bespoke_config *bespoke;
	struct esai_data *data;
	int ret;

	if (cfg->type != DAI_IMX_ESAI) {
		LOG_ERR("wrong DAI type: %d", cfg->type);
		return -EINVAL;
	}

	data = dev->data;
	bespoke = bespoke_data;

	/* TODO: does this function configure both the transmitter and the
	 * receiver?
	 */
	ret = esai_update_state(dir, data, DAI_STATE_READY);
	if (ret < 0) {
		return ret;
	}

	/* TODO: implement me */
	return 0;
}

static const struct dai_driver_api esai_api = {
	.config_set = esai_config_set,
};

static int esai_init(const struct device *dev)
{
	const struct esai_config *cfg;
	struct esai_data *data;

	cfg = dev->config;
	data = dev->data;

	device_map(&data->regmap, cfg->regmap_phys, cfg->regmap_size, K_MEM_CACHE_NONE);

	return 0;
}

#define ESAI_INIT(inst)							\
									\
static struct esai_config esai_config_##inst = {			\
	.regmap_phys = DT_INST_REG_ADDR(inst),				\
	.regmap_size = DT_INST_REG_SIZE(inst),				\
};									\
									\
static struct esai_data esai_data_##inst = {				\
	.cfg.type = DAI_IMX_ESAI,					\
	.cfg.dai_index = DT_INST_PROP_OR(inst, dai_index, 0),		\
};									\
									\
DEVICE_DT_INST_DEFINE(inst, &esai_init, NULL,				\
		      &esai_data_##inst, &esai_config_##inst,		\
		      POST_KERNEL, CONFIG_DAI_INIT_PRIORITY,		\
		      &esai_api);					\

DT_INST_FOREACH_STATUS_OKAY(ESAI_INIT);
