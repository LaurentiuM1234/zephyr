/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dma_nxp_edma.h"

/* used for driver binding */
#define DT_DRV_COMPAT nxp_edma

static void edma_isr(const void *parameter)
{
	const struct edma_config *cfg;
	struct edma_data *data;
	struct edma_channel *chan;
	DMA_Type *base;

	chan = (struct edma_channel *)parameter;
	cfg = chan->dev->config;
	data = chan->dev->data;
	base = UINT_TO_DMA(data->regmap);

	if (!(base->CH[chan->id].CH_INT & DMA_CH_INT_INT_MASK)) {
		/* skip, interrupt was probably triggered by another channel */
		return;
	}

	/* clear interrupt */
	base->CH[chan->id].CH_INT |= DMA_CH_INT_INT_MASK;

	/* TODO: are there any sanity checks we have to perform before invoking
	 * the registered callback?
	 */
	if (chan->cb) {
		chan->cb(chan->dev, chan->arg, chan->id, DMA_STATUS_COMPLETE);
	}
}

static struct edma_channel *lookup_channel(const struct device *dev,
					   uint32_t chan_id)
{
	struct edma_data *data;
	const struct edma_config *cfg;
	int i;

	data = dev->data;
	cfg = dev->config;


	/* optimization: if dma-channels property is present then
	 * the channel data associated with the passed channel ID
	 * can be found at index chan_id in the array of channels.
	 */
	if (cfg->contiguous_channels) {
		/* check for index out of bounds */
		if (chan_id >= data->ctx.dma_channels) {
			return NULL;
		}

		return &data->channels[chan_id];
	}

	/* channels are passed through the valid-channels property.
	 * As such, since some channels may be missing we need to
	 * look through the entire channels array for an ID match.
	 */
	for (i = 0; i < data->ctx.dma_channels; i++) {
		if (data->channels[i].id == chan_id) {
			return &data->channels[i];
		}
	}

	return NULL;
}

static int edma_config(const struct device *dev, uint32_t chan_id,
		       struct dma_config *dma_config)
{
	struct edma_data *data;
	const struct edma_config *cfg;
	struct edma_channel *chan;
	edma_transfer_type_t transfer_type;
	int ret;

	data = dev->data;
	cfg = dev->config;

	if (!dma_config->head_block) {
		LOG_ERR("head block shouldn't be NULL");
		return -EINVAL;
	}

	/* validate source data size (SSIZE) */
	if (!data_size_is_valid(dma_config->source_data_size)) {
		LOG_ERR("invalid source data size: %d",
			dma_config->source_data_size);
		return -EINVAL;
	}

	/* validate destination data size (DSIZE) */
	if (!data_size_is_valid(dma_config->dest_data_size)) {
		LOG_ERR("invalid destination data size: %d",
			dma_config->dest_data_size);
		return -EINVAL;
	}

	/* Scatter-Gather configurations currently not supported */
	if (dma_config->block_count != 1) {
		LOG_ERR("number of blocks %d not supported", dma_config->block_count);
		return -ENOTSUP;
	}

	/* check source address's (SADDR) alignment with respect to the data size (SSIZE) */
	if (dma_config->head_block->source_address % dma_config->source_data_size) {
		LOG_ERR("source address 0x%x alignment doesn't match data size %d",
			dma_config->head_block->source_address,
			dma_config->source_data_size);
		return -EINVAL;
	}

	/* check destination address's (DADDR) alignment with respect to the data size (DSIZE) */
	if (dma_config->head_block->dest_address % dma_config->dest_data_size) {
		LOG_ERR("destination address 0x%x alignment doesn't match data size %d",
			dma_config->head_block->dest_address,
			dma_config->dest_data_size);
		return -EINVAL;
	}

	/* source burst length should match destination burst length.
	 * This is because the burst length is the equivalent of NBYTES which
	 * is used to for both the destination and the source.
	 */
	if (dma_config->source_burst_length !=
	    dma_config->dest_burst_length) {
		LOG_ERR("source burst length %d doesn't match destination burst length %d",
			dma_config->source_burst_length,
			dma_config->dest_burst_length);
		return -EINVAL;
	}

	/* total number of bytes should be a multiple of NBYTES */
	if (dma_config->head_block->block_size % dma_config->source_burst_length) {
		LOG_ERR("block size %d should be a multiple of NBYTES %d",
			dma_config->head_block->block_size,
			dma_config->source_burst_length);
		return -EINVAL;
	}

	/* check if total number of bytes is a multiple of MAX(SSIZE, DSIZE).
	 * This condition stems from the following pieces of information:
	 *	1) SOFF and DOFF are set to SSIZE and DSIZE.
	 *	2) After each minor iteration, SOFF and DOFF are added to
	 *	SADDR and DADDR (*). As such, it's basically impossible to get
	 *	a number of bytes (NBYTES) that's not a multiple of these
	 *	values. (TODO: requires validation, currently an intuition)
	 *
	 * (*) this is only true for DOFF == SOFF. In other cases, SADDR and
	 * DADDR should be incremented by SGN_2ARGS(SOFF, DOFF) * MAX(ABS(SOFF),
	 * ABS(DOFF)), where SGN_2ARGS(SOFF, DOFF) = SGN(SOFF) in the case of
	 * SADDR and SGN_2ARGS(SOFF, DOFF) = SGN(DOFF) in the case of DADDR.
	 * This is because, if one of the values is smaller than the other
	 * multiple reads/writes will be performed until the higher value is
	 * reached. For example:
	 *	1) SOFF = 4, DOFF = 8 => 2 reads (4 bytes each) from SADDR will
	 *	be performed and a single write (8 bytes) to DADDR will be performed.
	 *
	 *	2) SOFF = 8, DOFF = 4, 1 read (8 bytes) from SADDR will be
	 *	performed and 2 write (4 bytes each) to DADDR will be performed.
	 *
	 * TODO: currently, we only support positive SOFF and DOFF. As such, if
	 * support for negative values is added in the future, we need to ABS
	 * the values of SOFF and DOFF before computing MAX.
	 */
	if (dma_config->source_burst_length %
	    MAX(dma_config->source_data_size, dma_config->dest_data_size)) {
		LOG_ERR("NBYTES %d should be a multiple of MAX(SSIZE(%d), DSIZE(%d))",
			dma_config->source_burst_length,
			dma_config->source_data_size,
			dma_config->dest_data_size);
		return -EINVAL;
	}

	/* fetch channel data */
	chan = lookup_channel(dev, chan_id);
	if (!chan) {
		LOG_ERR("channel ID %u is not valid", chan_id);
		return -EINVAL;
	}

	/* change channel's state to CONFIGURED */
	ret = channel_change_state(chan, CHAN_STATE_CONFIGURED);
	if (ret < 0) {
		LOG_ERR("failed to change channel %d state to CONFIGURED", chan_id);
		return ret;
	}

#ifdef CONFIG_DMA_NXP_EDMA_HAS_CHAN_MUX
	/* although the EDMA_HAS_CHAN_MUX feature is enabled, not all eDMA
	 * instances may have channel MUX-ing. As such, we also need to check
	 * for the presence of the channel-mux attribute to decide if we need
	 * to set the MUX value or not.
	 */
	if (cfg->channel_mux) {
		EDMA_SetChannelMux(UINT_TO_DMA(data->regmap),
				   chan_id, dma_config->dma_slot);
	}
#endif /* CONFIG_DMA_NXP_EDMA_HAS_CHAN_MUX */

	ret = get_transfer_type(dma_config->channel_direction, &transfer_type);
	if (ret < 0) {
		return ret;
	}

	chan->cb = dma_config->dma_callback;
	chan->arg = dma_config->user_data;

	/* warning: this sets SOFF and DOFF to SSIZE and DSIZE which are POSITIVE. */
	EDMA_PrepareTransfer(&chan->transfer_cfg,
			     UINT_TO_POINTER(dma_config->head_block->source_address),
			     dma_config->source_data_size,
			     UINT_TO_POINTER(dma_config->head_block->dest_address),
			     dma_config->dest_data_size,
			     dma_config->source_burst_length,
			     dma_config->head_block->block_size,
			     transfer_type);

	/* commit configuration */
	EDMA_SetTransferConfig(UINT_TO_DMA(data->regmap), chan_id,
			       &chan->transfer_cfg, NULL);

	/* set SLAST and DLAST */
	ret = set_slast_dlast(dma_config, transfer_type, data, chan_id);
	if (ret < 0) {
		return ret;
	}

	/* allow interrupting the CPU when half of a major cycle is completed
	 * and when a major cycle is completed.
	 */
	(UINT_TO_DMA(data->regmap))->CH[chan->id].TCD_CSR =
		DMA_TCD_CSR_INTMAJOR_MASK | DMA_TCD_CSR_INTHALF_MASK;

	/* enable channel interrupt */
	irq_enable(chan->irq);

	/* dump register status - for debugging purposes */
	edma_dump_channel_registers(data, chan_id);

	return 0;
}

/* WARNING: this function is bad and should be only used with SOF */
/* TODO: should we base the query on the transfer type? */
static int edma_get_status(const struct device *dev, uint32_t chan_id,
			   struct dma_status *stat)
{
	struct edma_data *data;
	struct edma_channel *chan;
	DMA_Type *base;

	data = dev->data;
	base = UINT_TO_DMA(data->regmap);

	/* fetch channel data */
	chan = lookup_channel(dev, chan_id);
	if (!chan) {
		LOG_ERR("channel ID %u is not valid", chan_id);
		return -EINVAL;
	}

	stat->free = abs(base->CH[chan_id].TCD_SLAST_SDA) / 2;
	stat->pending_length = abs(base->CH[chan_id].TCD_DLAST_SGA) / 2;

	return 0;
}

static int edma_stop(const struct device *dev, uint32_t chan_id)
{
	struct edma_data *data;
	const struct edma_config *cfg;
	struct edma_channel *chan;
	int ret;

	data = dev->data;
	cfg = dev->config;

	/* fetch channel data */
	chan = lookup_channel(dev, chan_id);
	if (!chan) {
		LOG_ERR("channel ID %u is not valid", chan_id);
		return -EINVAL;
	}

	/* change channel's state to STARTED */
	ret = channel_change_state(chan, CHAN_STATE_STOPPED);
	if (ret < 0) {
		LOG_ERR("failed to change channel %d state to STOPPED", chan_id);
		return ret;
	}

	LOG_DBG("stopping channel %u", chan_id);

	/* disable hardware requests */
	(UINT_TO_DMA(data->regmap))->CH[chan->id].CH_CSR &= ~DMA_CH_CSR_ERQ_MASK;

	return 0;
}

static int edma_start(const struct device *dev, uint32_t chan_id)
{
	struct edma_data *data;
	const struct edma_config *cfg;
	struct edma_channel *chan;
	int ret;

	data = dev->data;
	cfg = dev->config;

	/* fetch channel data */
	chan = lookup_channel(dev, chan_id);
	if (!chan) {
		LOG_ERR("channel ID %u is not valid", chan_id);
		return -EINVAL;
	}

	/* change channel's state to STARTED */
	ret = channel_change_state(chan, CHAN_STATE_STARTED);
	if (ret < 0) {
		LOG_ERR("failed to change channel %d state to STARTED", chan_id);
		return ret;
	}

	LOG_DBG("starting channel %u", chan_id);

	/* enable hardware requests */
	(UINT_TO_DMA(data->regmap))->CH[chan->id].CH_CSR = DMA_CH_CSR_ERQ_MASK;

	return 0;
}

static int edma_resume(const struct device *dev, uint32_t chan_id)
{
	/* nothing to be done here, just call dma_start() */
	return 0;
}

static bool edma_chan_filter(const struct device *dev, int chan_id, void *filter_data)
{
	enum dma_channel_filter filter = POINTER_TO_UINT(filter_data);

	/* note: only perform channel type validation here. It's mandatory
	 * to perform a channel data lookup during the other operations so
	 * might as well validate the channel ID there.
	 */
	if (filter == DMA_CHANNEL_NORMAL) {
		return true;
	}

	return false;
}

static const struct dma_driver_api edma_api = {
	.reload = NULL, /* TODO: should this be implemented? */
	.config = edma_config,
	.start = edma_start,
	.stop = edma_stop,
	.suspend = edma_stop,
	.resume = edma_resume,
	.get_status = edma_get_status,
	.chan_filter = edma_chan_filter,
};

static int edma_init(const struct device *dev)
{
	const struct edma_config *cfg;
	struct edma_data *data;
	edma_config_t edma_config;

	data = dev->data;
	cfg = dev->config;

	device_map(&data->regmap, cfg->regmap_phys, cfg->regmap_size, K_MEM_CACHE_NONE);

	cfg->irq_config();

	get_default_edma_config(&edma_config);

	EDMA_Init(UINT_TO_DMA(data->regmap), (const edma_config_t *)&edma_config);

	return 0;
}


/* a few comments about the BUILD_ASSERT statements:
 *	1) dma-channels and valid-channels should be mutually exclusive.
 *	This means that you specify the one or the other. There's no real
 *	need to have both of them.
 *	2) Number of channels should match the number of interrupts for
 *	said channels (TODO: what about error interrupts?)
 *	3) The channel-mux property shouldn't be specified unless
 *	the eDMA is MUX-capable (signaled via the EDMA_HAS_CHAN_MUX
 *	configuration).
 */
#define EDMA_INIT(inst)								\
										\
BUILD_ASSERT(!DT_NODE_HAS_PROP(DT_INST(inst, nxp_edma), dma_channels) ||	\
	     !DT_NODE_HAS_PROP(DT_INST(inst, nxp_edma), valid_channels),	\
	     "dma_channels and valid_channels are mutually exclusive");		\
										\
BUILD_ASSERT(DT_INST_PROP_OR(inst, dma_channels, 0) ==				\
	     DT_NUM_IRQS(DT_INST(inst, nxp_edma)) ||				\
	     DT_INST_PROP_LEN_OR(inst, valid_channels, 0) ==			\
	     DT_NUM_IRQS(DT_INST(inst, nxp_edma)),				\
	     "number of interrupts needs to match number of channels");		\
										\
BUILD_ASSERT(IS_ENABLED(CONFIG_DMA_NXP_EDMA_HAS_CHAN_MUX) ||			\
	     !DT_INST_PROP(inst, channel_mux),					\
	     "edma is not MUX-capable yet channel-mux is still specified?");	\
										\
static struct edma_channel channels_##inst[] = EDMA_CHANNEL_ARRAY_GET(inst);	\
										\
static void interrupt_config_function_##inst(void)				\
{										\
	EDMA_CONNECT_INTERRUPTS(inst);						\
}										\
										\
static struct edma_config edma_config_##inst = {				\
	.regmap_phys = DT_INST_REG_ADDR(inst),					\
	.regmap_size = DT_INST_REG_SIZE(inst),					\
	.irq_config = interrupt_config_function_##inst,				\
	.channel_mux = DT_INST_PROP(inst, channel_mux),				\
	.contiguous_channels = EDMA_CHANS_ARE_CONTIGUOUS(inst),			\
};										\
										\
static struct edma_data edma_data_##inst = {					\
	.channels = channels_##inst,						\
	.ctx.dma_channels = ARRAY_SIZE(channels_##inst),			\
	.ctx.magic = DMA_MAGIC,							\
};										\
										\
DEVICE_DT_INST_DEFINE(inst, &edma_init, NULL,					\
		      &edma_data_##inst, &edma_config_##inst,			\
		      PRE_KERNEL_1, CONFIG_DMA_INIT_PRIORITY,			\
		      &edma_api);						\

DT_INST_FOREACH_STATUS_OKAY(EDMA_INIT);
