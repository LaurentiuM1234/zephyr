/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/dma.h>
#include <zephyr/logging/log.h>

/* used for driver binding */
#define DT_DRV_COMPAT nxp_dummy_dma

LOG_MODULE_REGISTER(nxp_dummy_dma);

struct dummy_dma_data {
	/* this needs to be first */
	struct dma_context ctx;
};

static int dummy_dma_reload(const struct device *dev, uint32_t chan_id,
			    uint32_t src, uint32_t dst, size_t size)
{
	struct dummy_dma_data *data = dev->data;

	if (chan_id >= data->ctx.dma_channels) {
		LOG_ERR("channel %d is not a valid channel ID", chan_id);
		return -EINVAL;
	}

	/* TODO: any more validation to perform? */

	memcpy(UINT_TO_POINTER(src), UINT_TO_POINTER(dst), size);

	return 0;
}

static int dummy_dma_config(const struct device *dev, uint32_t chan_id,
		      struct dma_config *config)
{
	/* TODO: do I need to be implemented? */
	return 0;
}

static int dummy_dma_start(const struct device *dev, uint32_t chan_id)
{
	/* TODO: do I need to be implemented? */
	return 0;
}

static int dummy_dma_stop(const struct device *dev, uint32_t chan_id)
{
	/* TODO: do I need to be implemented? */
	return 0;
}

static int dummy_dma_suspend(const struct device *dev, uint32_t chan_id)
{
	/* TODO: do I need to be implemented? */
	return 0;
}

static int dummy_dma_resume(const struct device *dev, uint32_t chan_id)
{
	/* TODO: do I need to be implemented? */
	return 0;
}

static int dummy_dma_get_status(const struct device *dev, uint32_t chan_id,
				struct dma_status *stat)
{
	/* TODO: do I need to be implemented? */
	memset(stat, 0, sizeof(struct dma_status));

	return 0;
}

static bool dummy_dma_channel_filter(const struct device *dev,
				     int chan_id, void *filter_data)
{
	enum dma_channel_filter filter = POINTER_TO_UINT(filter_data);

	LOG_ERR("called channel filter");

	/* TODO: should we also validate the channel ID here? */
	if (filter == DMA_CHANNEL_NORMAL) {
		return true;
	}

	return false;
}

static const struct dma_driver_api dummy_dma_api = {
	.reload = dummy_dma_reload,
	.config = dummy_dma_config,
	.start = dummy_dma_start,
	.stop = dummy_dma_stop,
	.suspend = dummy_dma_suspend,
	.resume = dummy_dma_resume,
	.get_status = dummy_dma_get_status,
	.chan_filter = dummy_dma_channel_filter,
};

static struct dummy_dma_data dummy_dma_data = {
	.ctx.magic = DMA_MAGIC,
	.ctx.dma_channels = DT_INST_PROP(0, dma_channels),
};

/* assumption: only 1 DUMMY_DMA instance */
DEVICE_DT_INST_DEFINE(0, NULL, NULL,
		      &dummy_dma_data, NULL,
		      PRE_KERNEL_1, CONFIG_DMA_INIT_PRIORITY,
		      &dummy_dma_api);
