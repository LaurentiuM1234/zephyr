/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_

#include <zephyr/device.h>
#include <zephyr/drivers/firmware/scmi/shmem.h>

#define DT_INST_SCMI_SHMEM_BY_IDX(inst, idx)					\
	COND_CODE_1(DT_INST_PROP_HAS_IDX(inst, shmem, idx),			\
		    (DEVICE_DT_GET(DT_INST_PROP_BY_IDX(inst, shmem, idx))),	\
		    (NULL))

#define SCMI_TRANSPORT_PROLOGUE(inst)					\
	.transport_data.tx_shmem = DT_INST_SCMI_SHMEM_BY_IDX(inst, 0),	\
	.transport_data.rx_shmem = DT_INST_SCMI_SHMEM_BY_IDX(inst, 1)	\

#define SCMI_TRANSPORT_SHMEM_INFO\
	struct scmi_transport transport_data

#define SCMI_TRANSPORT_CHAN_SHMEM(data, tx)				\
	((tx) == 1 ? ((struct scmi_transport *)(data))->tx_shmem :	\
	 ((struct scmi_transport *)(data))->rx_shmem)

#define SCMI_TRANSPORT_SHMEM_NUM(inst)\
	DT_INST_PROP_LEN(inst, shmem)

#define SCMI_TRANSPORT_DBELL_NUM(inst)\
	DT_INST_PROP_LEN(inst, mboxes)

enum scmi_channel_type {
	SCMI_CHANNEL_TX = 0x0,
	SCMI_CHANNEL_RX = 0x1
};

struct scmi_transport {
	const struct device *tx_shmem;
	const struct device *rx_shmem;
};

struct scmi_channel;

struct scmi_transport_api {
	int (*request_channel)(const struct device *dev, int type,
			       struct scmi_channel *chan);
	int (*send_message)(struct scmi_channel *chan, struct scmi_message *msg);
	int (*send_message_async)(const struct device *dev, void *data);
};

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_ */
