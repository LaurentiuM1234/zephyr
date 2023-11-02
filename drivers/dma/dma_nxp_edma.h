/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_DMA_DMA_NXP_EDMA_H_
#define ZEPHYR_DRIVERS_DMA_DMA_NXP_EDMA_H_

#include <zephyr/device.h>
#include <zephyr/irq.h>
#include <zephyr/drivers/dma.h>
#include <fsl_edma.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nxp_edma);

/* workaround the fact that device_map() is not defined for SoCs with no MMU */
#ifndef DEVICE_MMIO_IS_IN_RAM
#define device_map(virt, phys, size, flags) *(virt) = (phys)
#endif /* DEVICE_MMIO_IS_IN_RAM */

/* macros used to parse DTS properties */

/* used in conjunction with LISTIFY which expects F to also take a variable
 * number of arguments. Since IDENTITY doesn't do that we need to use a version
 * of it which also takes a variable number of arguments.
 */
#define IDENTITY_VARGS(V, ...) IDENTITY(V)

/* used to generate an array of indexes for the channels */
#define _EDMA_CHANNEL_INDEX_ARRAY(inst)\
	LISTIFY(DT_INST_PROP_LEN_OR(inst, valid_channels, 0), IDENTITY_VARGS, (,))

/* used to generate an array of indexes for the channels - this is different
 * from _EDMA_CHANNEL_INDEX_ARRAY because the number of channels is passed
 * explicitly through dma-channels so no need to deduce it from the length
 * of the valid-channels property.
 */
#define _EDMA_CHANNEL_INDEX_ARRAY_EXPLICIT(inst)\
	LISTIFY(DT_INST_PROP_OR(inst, dma_channels, 0), IDENTITY_VARGS, (,))

/* used to generate an array of indexes for the interrupt */
#define _EDMA_INT_INDEX_ARRAY(inst)\
	LISTIFY(DT_NUM_IRQS(DT_INST(inst, nxp_edma)), IDENTITY_VARGS, (,))

/* used to register an ISR/arg pair. TODO: should we also use the priority? */
#define _EDMA_INT_CONNECT(idx, inst)				\
	IRQ_CONNECT(DT_INST_IRQ_BY_IDX(inst, idx, irq),		\
		    0, edma_isr,				\
		    &channels_##inst[idx], 0)

/* used to declare a struct edma_channel by the non-explicit macro suite */
#define _EDMA_CHANNEL_DECLARE(idx, inst)				\
{									\
	.id = DT_INST_PROP_BY_IDX(inst, valid_channels, idx),		\
	.dev = DEVICE_DT_GET(DT_INST(inst, nxp_edma)),			\
	.irq = DT_INST_IRQ_BY_IDX(inst, idx, irq),			\
}

/* used to declare a struct edma_channel by the explicit macro suite */
#define _EDMA_CHANNEL_DECLARE_EXPLICIT(idx, inst)			\
{									\
	.id = idx,							\
	.dev = DEVICE_DT_GET(DT_INST(inst, nxp_edma)),			\
	.irq = DT_INST_IRQ_BY_IDX(inst, idx, irq),			\
}

/* used to create an array of channel IDs via the valid-channels property */
#define _EDMA_CHANNEL_ARRAY(inst)					\
	{ FOR_EACH_FIXED_ARG(_EDMA_CHANNEL_DECLARE, (,),		\
			     inst, _EDMA_CHANNEL_INDEX_ARRAY(inst)) }

/* used to create an array of channel IDs via the dma-channels property */
#define _EDMA_CHANNEL_ARRAY_EXPLICIT(inst)				\
	{ FOR_EACH_FIXED_ARG(_EDMA_CHANNEL_DECLARE_EXPLICIT, (,), inst,	\
			     _EDMA_CHANNEL_INDEX_ARRAY_EXPLICIT(inst)) }

/* used to construct the channel array based on the specified property:
 * dma-channels or valid-channels.
 */
#define EDMA_CHANNEL_ARRAY_GET(inst)						\
	COND_CODE_1(DT_NODE_HAS_PROP(DT_INST(inst, nxp_edma), dma_channels),	\
		    (_EDMA_CHANNEL_ARRAY_EXPLICIT(inst)),			\
		    (_EDMA_CHANNEL_ARRAY(inst)))

/* used to register edma_isr for all specified interrupts */
#define EDMA_CONNECT_INTERRUPTS(inst)				\
	FOR_EACH_FIXED_ARG(_EDMA_INT_CONNECT, (;),		\
			   inst, _EDMA_INT_INDEX_ARRAY(inst))

#define EDMA_CHANS_ARE_CONTIGUOUS(inst)\
	DT_NODE_HAS_PROP(DT_INST(inst, nxp_edma), dma_channels)

/* utility macros */
#define UINT_TO_DMA(regmap) ((DMA_Type *)(regmap))

enum channel_state {
	CHAN_STATE_INIT = 0,
	CHAN_STATE_CONFIGURED,
	CHAN_STATE_STARTED,
	CHAN_STATE_STOPPED,
	CHAN_STATE_SUSPENDED,
};

struct edma_channel {
	uint32_t id;
	const struct device *dev;
	enum channel_state state;
	edma_transfer_config_t transfer_cfg;
	void *arg;
	dma_callback_t cb;
	int irq;
};

struct edma_data {
	/* this needs to be the first member */
	struct dma_context ctx;
	mm_reg_t regmap;
	struct edma_channel *channels;
	atomic_t channel_flags;
};

struct edma_config {
	uint32_t regmap_phys;
	uint32_t regmap_size;
	void (*irq_config)(void);
	bool channel_mux;
	bool contiguous_channels;
};

static inline int channel_change_state(struct edma_channel *chan,
				       enum channel_state next)
{
	enum channel_state prev = chan->state;

	LOG_DBG("attempting to change state from %d to %d for channel %d", prev, next, chan->id);

	/* validate transition */
	switch (prev) {
	case CHAN_STATE_INIT:
		if (next != CHAN_STATE_CONFIGURED) {
			return -EPERM;
		}
		break;
	case CHAN_STATE_CONFIGURED:
		if (next != CHAN_STATE_CONFIGURED &&
		    next != CHAN_STATE_STARTED) {
			return -EPERM;
		}
		break;
	case CHAN_STATE_STARTED:
		if (next != CHAN_STATE_STOPPED &&
		    next != CHAN_STATE_SUSPENDED) {
			return -EPERM;
		}
		break;
	case CHAN_STATE_STOPPED:
		if (next != CHAN_STATE_CONFIGURED) {
			return -EPERM;
		}
		break;
	case CHAN_STATE_SUSPENDED:
		if (next != CHAN_STATE_STARTED &&
		    next != CHAN_STATE_STOPPED) {
			return -EPERM;
		}
		break;
	default:
		LOG_ERR("invalid channel previous state: %d", prev);
		return -EINVAL;
	}

	/* transition OK, proceed */
	chan->state = next;

	return 0;
}

static inline void get_default_edma_config(edma_config_t *edma_config)
{
	memset(edma_config, 0, sizeof(edma_config_t));
}

static inline int get_transfer_type(enum dma_channel_direction dir,
				    edma_transfer_type_t *type)
{
	switch (dir) {
	case MEMORY_TO_MEMORY:
		*type = kEDMA_MemoryToMemory;
		break;
	case MEMORY_TO_PERIPHERAL:
		*type = kEDMA_MemoryToPeripheral;
		break;
	case PERIPHERAL_TO_MEMORY:
		*type = kEDMA_PeripheralToMemory;
		break;
	default:
		LOG_ERR("invalid channel direction: %d", dir);
		return -EINVAL;
	}

	return 0;
}

static inline bool data_size_is_valid(uint16_t size)
{
	switch (size) {
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
		break;
	default:
		return false;
	}

	return true;
}

static inline void edma_dump_channel_registers(struct edma_data *data,
					       uint32_t chan_id)
{
	DMA_Type *base = UINT_TO_DMA(data->regmap);

	LOG_ERR("dumping channel data for channel %d", chan_id);

	LOG_ERR("CH_CSR: 0x%x", base->CH[chan_id].CH_CSR);
	LOG_ERR("CH_ES: 0x%x", base->CH[chan_id].CH_ES);
	LOG_ERR("CH_INT: 0x%x", base->CH[chan_id].CH_INT);
	LOG_ERR("CH_SBR: 0x%x", base->CH[chan_id].CH_SBR);
	LOG_ERR("CH_PRI: 0x%x", base->CH[chan_id].CH_PRI);
	LOG_ERR("CH_MUX: 0x%x", base->CH[chan_id].CH_MUX);

	LOG_ERR("TCD_SADDR: 0x%x", base->CH[chan_id].TCD_SADDR);
	LOG_ERR("TCD_SOFF: 0x%x", base->CH[chan_id].TCD_SOFF);
	LOG_ERR("TCD_ATTR: 0x%x", base->CH[chan_id].TCD_ATTR);
	LOG_ERR("TCD_NBYTES_MLOFFNO: 0x%x", base->CH[chan_id].TCD_NBYTES_MLOFFNO);
	LOG_ERR("TCD_SLAST_SDA: 0x%x", base->CH[chan_id].TCD_SLAST_SDA);

	LOG_ERR("TCD_DADDR: 0x%x", base->CH[chan_id].TCD_DADDR);
	LOG_ERR("TCD_DOFF: 0x%x", base->CH[chan_id].TCD_DOFF);

	LOG_ERR("TCD_CITER_ELINKNO: 0x%x", base->CH[chan_id].TCD_CITER_ELINKNO);
	LOG_ERR("TCD_DLAST_SGA 0x%x", base->CH[chan_id].TCD_DLAST_SGA);

	LOG_ERR("TCD_CSR 0x%x", base->CH[chan_id].TCD_CSR);

	LOG_ERR("TCD_BITER_ELINKNO 0x%x", base->CH[chan_id].TCD_BITER_ELINKNO);
}

static inline int set_slast_dlast(struct dma_config *dma_config,
				  edma_transfer_type_t transfer_type,
				  struct edma_data *data,
				  uint32_t chan_id)
{
	DMA_Type *base;
	int32_t slast, dlast;

	base = UINT_TO_DMA(data->regmap);

	if (transfer_type == kEDMA_PeripheralToMemory) {
		slast = 0;
	} else {
		switch (dma_config->head_block->source_addr_adj) {
		case DMA_ADDR_ADJ_INCREMENT:
			slast = (int32_t)dma_config->head_block->block_size;
			break;
		case DMA_ADDR_ADJ_DECREMENT:
			slast = (-1) * (int32_t)dma_config->head_block->block_size;
			break;
		default:
			LOG_ERR("unsupported SADDR adjustment: %d",
				dma_config->head_block->source_addr_adj);
			return -EINVAL;
		}
	}

	if (transfer_type == kEDMA_MemoryToPeripheral) {
		dlast = 0;
	} else {
		switch (dma_config->head_block->dest_addr_adj) {
		case DMA_ADDR_ADJ_INCREMENT:
			dlast = (int32_t)dma_config->head_block->block_size;
			break;
		case DMA_ADDR_ADJ_DECREMENT:
			dlast = (-1) * (int32_t)dma_config->head_block->block_size;
			break;
		default:
			LOG_ERR("unsupported DADDR adjustment: %d",
				dma_config->head_block->dest_addr_adj);
			return -EINVAL;
		}
	}

	LOG_DBG("attempting to commit SLAST %d", slast);
	LOG_DBG("attempting to commit DLAST %d", dlast);

	/* commit configuration */
	base->CH[chan_id].TCD_SLAST_SDA = slast;
	base->CH[chan_id].TCD_DLAST_SGA = dlast;

	return 0;
}

#endif /* ZEPHYR_DRIVERS_DMA_DMA_NXP_EDMA_H_ */
