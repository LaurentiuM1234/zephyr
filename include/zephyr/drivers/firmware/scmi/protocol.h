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

#define _SCMI_PROTOCOL_NAME(proto) CONCAT(scmi_protocol_, proto)

#define _SCMI_PROTOCOL_DATA_DECLARE(node_id, proto, data)			\
	STRUCT_SECTION_ITERABLE(scmi_protocol, _SCMI_PROTOCOL_NAME(proto)) =	\
	{									\
		.id = proto,							\
		.tx = SCMI_TRANSPORT_GET_TX_CHAN(node_id, proto),		\
		.rx = SCMI_TRANSPORT_GET_RX_CHAN(node_id, proto),		\
		.priv = data,							\
	}


#define DT_SCMI_PROTOCOL_NAME(node_id)\
	_SCMI_PROTOCOL_NAME(DT_REG_ADDR(node_id))

#define DT_INST_SCMI_PROTOCOL_NAME(inst)\
	DT_SCMI_PROTOCOL_NAME(DT_INST(inst, DT_DRV_COMPAT))

#define DT_INST_SCMI_PROTOCOL_DEFINE(inst, init, data, cfg, prio, api)		\
	SCMI_TRANSPORT_TX_CHAN_DECLARE_EXT(DT_INST(inst, DT_DRV_COMPAT),	\
					   DT_INST_REG_ADDR(inst))		\
										\
	SCMI_TRANSPORT_RX_CHAN_DECLARE_EXT(DT_INST(inst, DT_DRV_COMPAT),	\
					   DT_INST_REG_ADDR(inst))		\
										\
	_SCMI_PROTOCOL_DATA_DECLARE(DT_INST(inst, DT_DRV_COMPAT),		\
				    DT_INST_REG_ADDR(inst),			\
				    data);					\
										\
DEVICE_DT_INST_DEFINE(inst, init, NULL, &DT_INST_SCMI_PROTOCOL_NAME(inst),	\
		      cfg, POST_KERNEL, prio, api);

#define DT_SCMI_PROTOCOL_DEFINE(node_id, data)					\
	SCMI_TRANSPORT_TX_CHAN_DECLARE_EXT(node_id, DT_REG_ADDR(node_id))	\
	SCMI_TRANSPORT_RX_CHAN_DECLARE_EXT(node_id, DT_REG_ADDR(node_id))	\
	_SCMI_PROTOCOL_DATA_DECLARE(node_id, DT_REG_ADDR(node_id), data)


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
	void *priv;
};

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_ */
