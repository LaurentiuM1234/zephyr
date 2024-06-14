/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_

#include <zephyr/device.h>
#include <zephyr/drivers/firmware/scmi/protocol.h>
#include <zephyr/drivers/firmware/scmi/common.h>

#ifdef CONFIG_ARM_SCMI

#define _SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, tx)\
	DT_PROP_HAS_IDX(node_id, shmem, tx)

#endif /* CONFIG_ARM_SCMI */

#define SCMI_TRANSPORT_CHAN_NAME(proto, tx)\
	CONCAT(scmi_channel_, proto, _, tx)

#define SCMI_TRANSPORT_CHAN_DEFINE(node_id, proto, tx, transport_priv)	\
	STRUCT_SECTION_ITERABLE(scmi_channel,				\
				SCMI_TRANSPORT_CHAN_NAME(proto, tx)) =	\
	{								\
		.priv = transport_priv,					\
	}

#define DT_SCMI_TRANSPORT_RX_BASE_CHAN(node_id)					\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(DT_PARENT(node_id), 1),	\
		    (&SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 1)),		\
		    (NULL))

#define DT_SCMI_TRANSPORT_TX_CHAN(node_id)					\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 0),			\
		    (&SCMI_TRANSPORT_CHAN_NAME(DT_REG_ADDR(node_id), 0)),	\
		    (&SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 0)))

#define DT_SCMI_TRANSPORT_RX_CHAN(node_id)					\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 1),			\
		    (&SCMI_TRANSPORT_CHAN_NAME(DT_REG_ADDR(node_id), 1)),	\
		    (DT_SCMI_TRANSPORT_RX_BASE_CHAN(node_id)))

#define DT_SCMI_TRANSPORT_TX_CHAN_NAME(node_id)					\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 0),			\
		    (SCMI_TRANSPORT_CHAN_NAME(DT_REG_ADDR(node_id), 0)),	\
		    (SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 0)))		\

#define _DT_SCMI_TRANSPORT_TX_CHAN_DECLARE(node_id, proto)\
	extern struct scmi_channel DT_SCMI_TRANSPORT_TX_CHAN_NAME(node_id);

#define _SCMI_TRANSPORT_RX_CHAN_DECLARE_OPTIONAL(node_id, proto)			\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 1),				\
		    (extern struct scmi_channel SCMI_TRANSPORT_CHAN_NAME(proto, 1);),	\
		    ())

#define _DT_SCMI_TRANSPORT_RX_CHAN_DECLARE(node_id, proto)				\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 1),				\
		    (_SCMI_TRANSPORT_RX_CHAN_DECLARE(node_id, proto)),			\
		    (_SCMI_TRANSPORT_RX_CHAN_DECLARE_OPTIONAL(DT_PARENT(node_id),	\
							      SCMI_PROTOCOL_BASE)))

#define DT_INST_SCMI_TRANSPORT_DEFINE(inst, pm, data, config, level, prio, api)	\
	DEVICE_DT_INST_DEFINE(inst, &scmi_core_transport_init,			\
			      pm, data, config, level, prio, api)

#define DT_SCMI_TRANSPORT_CHANNELS_DECLARE(node_id)				\
	_DT_SCMI_TRANSPORT_TX_CHAN_DECLARE(node_id, DT_REG_ADDR(node_id))	\
	_DT_SCMI_TRANSPORT_RX_CHAN_DECLARE(node_id, DT_REG_ADDR(node_id))

#define DT_INST_SCMI_TRANSPORT_CHANNELS_DECLARE(inst)\
	DT_SCMI_TRANSPORT_CHANNELS_DECLARE(DT_INST(inst, DT_DRV_COMPAT))

struct scmi_transport_api {
	int (*init)(const struct device *transport);
	int (*send_message)(const struct device *transport,
			    struct scmi_channel *chan,
			    struct scmi_message *msg);
	int (*setup_chan)(const struct device *transport,
			  struct scmi_channel *chan,
			  bool tx);
	int (*read_message)(const struct device *transport,
			    struct scmi_channel *chan,
			    struct scmi_message *msg);
};

static inline int scmi_transport_init(const struct device *transport)
{
	const struct scmi_transport_api *api =
		(const struct scmi_transport_api *)transport->api;

	if (api->init) {
		return api->init(transport);
	}

	return 0;
}

static inline int scmi_transport_setup_chan(const struct device *transport,
					    struct scmi_channel *chan,
					    bool tx)
{
	const struct scmi_transport_api *api =
		(const struct scmi_transport_api *)transport->api;

	if (!api || !api->setup_chan) {
		return -ENOSYS;
	}

	return api->setup_chan(transport, chan, tx);
}

static inline int scmi_transport_send_message(const struct device *transport,
					      struct scmi_channel *chan,
					      struct scmi_message *msg)
{
	const struct scmi_transport_api *api =
		(const struct scmi_transport_api *)transport->api;

	if (!api || !api->send_message) {
		return -ENOSYS;
	}

	return api->send_message(transport, chan, msg);
}

static inline int scmi_transport_read_message(const struct device *transport,
					      struct scmi_channel *chan,
					      struct scmi_message *msg)
{
	const struct scmi_transport_api *api =
		(const struct scmi_transport_api *)transport->api;

	if (!api || !api->read_message) {
		return -ENOSYS;
	}

	return api->read_message(transport, chan, msg);
}

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_ */
