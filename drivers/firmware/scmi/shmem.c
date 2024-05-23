/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/firmware/scmi/shmem.h>
#include <string.h>
#include <errno.h>

#define DT_DRV_COMPAT arm_scmi_shmem

#ifndef DEVICE_MMIO_IS_IN_RAM
#define device_map(virt, phys, size, flags) *(virt) = (phys)
#endif /* DEVICE_MMIO_IS_IN_RAM */

#define SCMI_SHMEM_CHANNEL_BUSY_MASK BIT(0)
#define SCMI_SHMEM_CHANNEL_ERROR_MASK BIT(1)

struct scmi_shmem_config {
	uintptr_t phys_addr;
	uint32_t size;
};

struct scmi_shmem_data {
	mm_reg_t addr;
};

struct scmi_shmem_layout {
	uint32_t reserved0;
	uint32_t channel_status;
	uint32_t reserved1[2];
	uint32_t channel_flags;
	uint32_t length;
	uint32_t message_header;
	uint32_t message_payload_start;
};

int scmi_shmem_is_busy(const struct device *dev)
{
	struct scmi_shmem_data *data = dev->data;
	struct scmi_shmem_layout *layout;

	data = dev->data;
	layout = (struct scmi_shmem_layout *)data->addr;

	return !(layout->channel_status & SCMI_SHMEM_CHANNEL_BUSY_MASK);
}

void scmi_shmem_update_channel_status(const struct device *dev, uint32_t set,
				      uint32_t clear)
{
	struct scmi_shmem_data *data = dev->data;
	struct scmi_shmem_layout *layout;

	data = dev->data;
	layout = (struct scmi_shmem_layout *)data->addr;

	layout->channel_status &= ~clear;
	layout->channel_status |= set;
}

void scmi_shmem_update_channel_flags(const struct device *dev, uint32_t set,
				     uint32_t clear)
{
	struct scmi_shmem_data *data = dev->data;
	struct scmi_shmem_layout *layout;

	data = dev->data;
	layout = (struct scmi_shmem_layout *)data->addr;

	layout->channel_flags &= ~clear;
	layout->channel_flags |= set;
}

int scmi_shmem_read_message(const struct device *dev, struct scmi_message *msg)
{
	const struct scmi_shmem_config *cfg;
	struct scmi_shmem_data *data;
	struct scmi_shmem_layout *layout;

	cfg = dev->config;
	data = dev->data;
	layout = (struct scmi_shmem_layout *)data->addr;

	if (msg->length != (layout->length - sizeof(layout->message_header))) {
		return -EINVAL;
	}

	msg->hdr = layout->message_header;
	memcpy(msg->data, &layout->message_payload_start, msg->length);

	return 0;
}

int scmi_shmem_write_message(const struct device *dev, struct scmi_message *msg)
{
	const struct scmi_shmem_config *cfg;
	struct scmi_shmem_data *data;
	struct scmi_shmem_layout *layout;

	cfg = dev->config;
	data = dev->data;
	layout = (struct scmi_shmem_layout *)data->addr;

	if (!msg) {
		return -EINVAL;
	}

	if (msg->length && !msg->data) {
		return -EINVAL;
	}

	/* will the message fit? */
	if (cfg->size < msg->length + sizeof(layout->message_header)) {
		return -EINVAL;
	}

	/* clear channel error bit */
	layout->channel_status &= ~SCMI_SHMEM_CHANNEL_ERROR_MASK;

	/* write message content */
	layout->length = msg->length + sizeof(layout->message_header);
	layout->message_header = msg->hdr;

	if (msg->length) {
		memcpy(&layout->message_payload_start, msg->data, msg->length);
	}

	/* mark channel as busy */
	layout->channel_status &= ~SCMI_SHMEM_CHANNEL_BUSY_MASK;

	return 0;
}

static int scmi_shmem_init(const struct device *dev)
{
	const struct scmi_shmem_config *cfg;
	struct scmi_shmem_data *data;

	cfg = dev->config;
	data = dev->data;

	device_map(&data->addr, cfg->phys_addr, cfg->size, K_MEM_CACHE_NONE);

	return 0;
}

#define SCMI_SHMEM_INIT(inst)							\
										\
static const struct scmi_shmem_config scmi_shmem_config_##inst = {		\
	.phys_addr = DT_INST_REG_ADDR(inst),					\
	.size = DT_INST_REG_SIZE(inst),						\
};										\
										\
static struct scmi_shmem_data scmi_shmem_data_##inst;				\
										\
DEVICE_DT_INST_DEFINE(inst, &scmi_shmem_init, NULL,				\
		      &scmi_shmem_data_##inst, &scmi_shmem_config_##inst,	\
		      PRE_KERNEL_1, CONFIG_ARM_SCMI_SHMEM_INIT_PRIORITY,	\
		      NULL);

DT_INST_FOREACH_STATUS_OKAY(SCMI_SHMEM_INIT);
