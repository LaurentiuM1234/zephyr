/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/dma.h>
#include <zephyr/logging/log.h>

/* used for driver binding */
#define DT_DRV_COMPAT nxp_dummy_dma

/* macros used to parse DTS properties */
#define IDENTITY_VARGS(V, ...) IDENTITY(V)

#define _DUMMY_DMA_CHANNEL_INDEX_ARRAY(inst)\
	LISTIFY(DT_INST_PROP_OR(inst, dma_channels, 0), IDENTITY_VARGS, (,))

#define _DUMMY_DMA_CHANNEL_DECLARE(idx) {}

#define DUMMY_DMA_CHANNELS_DECLARE(inst)\
	FOR_EACH(_DUMMY_DMA_CHANNEL_DECLARE,\
		 (,), _DUMMY_DMA_CHANNEL_INDEX_ARRAY(inst))

LOG_MODULE_REGISTER(nxp_dummy_dma);

enum channel_state {
	CHAN_STATE_INIT = 0,
	CHAN_STATE_CONFIGURED,
};

struct dummy_dma_channel {
	uint32_t src;
	uint32_t dest;
	uint32_t size;
	enum channel_state state;
};

struct dummy_dma_data {
	/* this needs to be first */
	struct dma_context ctx;
	atomic_t channel_flags;
	struct dummy_dma_channel *channels;
};

static int channel_change_state(struct dummy_dma_channel *chan,
				enum channel_state next)
{
	enum channel_state prev = chan->state;

	/* validate transition */
	switch (prev) {
	case CHAN_STATE_INIT:
	case CHAN_STATE_CONFIGURED:
		if (next != CHAN_STATE_CONFIGURED) {
			return -EPERM;
		}
		break;
	default:
		LOG_ERR("invalid channel previous state: %d", prev);
		return -EINVAL;
	}

	chan->state = next;

	return 0;
}

static int dummy_dma_reload(const struct device *dev, uint32_t chan_id,
			    uint32_t src, uint32_t dst, size_t size)
{
	ARG_UNUSED(src);
	ARG_UNUSED(dst);
	ARG_UNUSED(size);

	struct dummy_dma_data *data;
	struct dummy_dma_channel *chan;

	data = dev->data;

	if (chan_id >= data->ctx.dma_channels) {
		LOG_ERR("channel %d is not a valid channel ID", chan_id);
		return -EINVAL;
	}

	/* fetch channel data */
	chan = &data->channels[chan_id];

	/* validate state */
	if (chan->state != CHAN_STATE_CONFIGURED) {
		LOG_ERR("attempting to reload unconfigured DMA channel %d", chan_id);
		return -EINVAL;
	}

	LOG_DBG("attempting copy for channel %d with SADDR 0x%x, DADDR 0x%x, and SIZE 0x%x",
		chan_id, chan->src, chan->dest, chan->size);

	memcpy(UINT_TO_POINTER(chan->dest), UINT_TO_POINTER(chan->src), chan->size);

	return 0;
}


static int dummy_dma_config(const struct device *dev, uint32_t chan_id,
		      struct dma_config *config)
{
	struct dummy_dma_data *data;
	struct dummy_dma_channel *chan;
	int ret;

	data = dev->data;

	if (chan_id >= data->ctx.dma_channels) {
		LOG_ERR("channel %d is not a valid channel ID", chan_id);
		return -EINVAL;
	}

	/* fetch channel data */
	chan = &data->channels[chan_id];

	/* attempt a state transition */
	ret = channel_change_state(chan, CHAN_STATE_CONFIGURED);
	if (ret < 0) {
		LOG_ERR("failed to change channel %d's state to CONFIGURED", chan_id);
		return ret;
	}

	if (config->block_count != 1) {
		LOG_ERR("invalid number of blocks: %d", config->block_count);
		return -EINVAL;
	}

	if (!config->head_block->source_address) {
		LOG_ERR("got NULL source address");
		return -EINVAL;
	}

	if (!config->head_block->dest_address) {
		LOG_ERR("got NULL destination address");
		return -EINVAL;
	}

	if (!config->head_block->block_size) {
		LOG_ERR("got 0 bytes to copy");
		return -EINVAL;
	}

	if (config->channel_direction != HOST_TO_MEMORY &&
	    config->channel_direction != MEMORY_TO_HOST) {
		LOG_ERR("invalid channel direction: %d",
			config->channel_direction);
		return -EINVAL;
	}

	/* SSIZE and DSIZE should have the same value */
	if (config->source_data_size != config->dest_data_size) {
		LOG_ERR("SSIZE %d and DSIZE %d should have the same value");
		return -EINVAL;
	}

	/* latch onto the passed configuration */
	chan->src = config->head_block->source_address;
	chan->dest = config->head_block->dest_address;
	chan->size = config->head_block->block_size;

	LOG_DBG("configured channel %d with SRC 0x%x DST 0x%x SIZE 0x%x",
		chan_id, chan->src, chan->dest, chan->size);

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

static int dummy_dma_get_attribute(const struct device *dev, uint32_t type, uint32_t *val)
{
	switch (type) {
	case DMA_ATTR_COPY_ALIGNMENT:
	case DMA_ATTR_BUFFER_SIZE_ALIGNMENT:
		*val = CONFIG_DMA_NXP_DUMMY_DMA_MAX_ALIGN / 8;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*val = CONFIG_DMA_NXP_DUMMY_DMA_MAX_ALIGN;
		break;
	default:
		LOG_ERR("invalid attribute type: %d", type);
		return -EINVAL;
	}

	return 0;
}

static const struct dma_driver_api dummy_dma_api = {
	.reload = dummy_dma_reload,
	.config = dummy_dma_config,
	.start = dummy_dma_start,
	.stop = dummy_dma_stop,
	.suspend = dummy_dma_suspend,
	.resume = dummy_dma_resume,
	.get_status = dummy_dma_get_status,
	.get_attribute = dummy_dma_get_attribute,
};

static int dummy_dma_init(const struct device *dev)
{
	struct dummy_dma_data *data = dev->data;

	data->channel_flags = ATOMIC_INIT(data->ctx.dma_channels);
	data->ctx.atomic = &data->channel_flags;

	return 0;
}

static struct dummy_dma_channel channels[] = {
	DUMMY_DMA_CHANNELS_DECLARE(0),
};

static struct dummy_dma_data dummy_dma_data = {
	.ctx.magic = DMA_MAGIC,
	.ctx.dma_channels = ARRAY_SIZE(channels),
	.channels = channels,
};

/* assumption: only 1 DUMMY_DMA instance */
DEVICE_DT_INST_DEFINE(0, dummy_dma_init, NULL,
		      &dummy_dma_data, NULL,
		      PRE_KERNEL_1, CONFIG_DMA_INIT_PRIORITY,
		      &dummy_dma_api);
