/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_

#include <zephyr/device.h>
#include <zephyr/drivers/firmware/scmi/transport.h>
#include <zephyr/drivers/firmware/scmi/common.h>

#define _DT_SCMI_PROTOCOL_DATA_DEFINE(node_id, data)				\
	STRUCT_SECTION_ITERABLE(scmi_protocol, DT_SCMI_PROTOCOL_NAME(node_id))=	\
	{									\
		.id = DT_REG_ADDR(node_id),					\
		.rx = DT_SCMI_TRANSPORT_RX_CHAN(node_id),			\
		.tx = DT_SCMI_TRANSPORT_TX_CHAN(node_id),			\
		.priv = data,							\
	}

#define DT_SCMI_PROTOCOL_NAME(node_id)\
	CONCAT(scmi_protocol_, DT_REG_ADDR(node_id))

#define DT_INST_SCMI_PROTOCOL_NAME(inst)\
	DT_SCMI_PROTOCOL_NAME(DT_INST(inst, DT_DRV_COMPAT))

#define DT_SCMI_PROTOCOL_DEFINE(node_id, init_fn, pm, data, config, level, prio, api)	\
	DT_SCMI_TRANSPORT_CHANNELS_DECLARE(node_id)					\
	_DT_SCMI_PROTOCOL_DATA_DEFINE(node_id, data);					\
	DEVICE_DT_DEFINE(node_id, init_fn, pm, data, config, level, prio, api)

#define DT_INST_SCMI_PROTOCOL_DEFINE(inst, init_fn, pm, data, config, level, prio, api)	\
	DT_SCMI_PROTOCOL_DEFINE(DT_INST(inst, DT_DRV_COMPAT), init_fn,			\
				pm, data, config, level, prio, api)

#define DT_SCMI_PROTOCOL_DEFINE_NODEV(node_id, data)				\
	DT_SCMI_TRANSPORT_CHANNELS_DECLARE(node_id)				\
	_DT_SCMI_PROTOCOL_DATA_DEFINE(node_id, data)


/* TODO: add note about this being in decimal */
#define SCMI_PROTOCOL_BASE 16
#define SCMI_PROTOCOL_POWER_DOMAIN 17
#define SCMI_PROTOCOL_SYSTEM 18
#define SCMI_PROTOCOL_PERF 19
#define SCMI_PROTOCOL_CLOCK 20
#define SCMI_PROTOCOL_SENSOR 21
#define SCMI_PROTOCOL_RESET_DOMAIN 22
#define SCMI_PROTOCOL_VOLTAGE_DOMAIN 23

struct scmi_protocol {
	uint32_t id;
	struct scmi_channel *tx;
	struct scmi_channel *rx;
	const struct device *transport;
	void *priv;
};

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_ */
