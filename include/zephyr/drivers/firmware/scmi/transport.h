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

#define SCMI_TRANSPORT_CHAN_DECLARE(node_id, proto, tx, transport_priv)	\
	STRUCT_SECTION_ITERABLE(scmi_channel,				\
				SCMI_TRANSPORT_CHAN_NAME(proto, tx)) =	\
	{								\
		.priv = transport_priv,					\
	}

#define _SCMI_TRANSPORT_GET_TX_BASE_CHAN(node_id)\
	(&(SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 0)))

#define _SCMI_TRANSPORT_GET_RX_BASE_CHAN(node_id)				\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(DT_PARENT(node_id), 1),	\
		    (&(SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 1))),	\
		    (NULL))

#define SCMI_TRANSPORT_GET_TX_CHAN(node_id, proto)			\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 0),		\
		    (&(SCMI_TRANSPORT_CHAN_NAME(proto, 0))),		\
		    (_SCMI_TRANSPORT_GET_TX_BASE_CHAN(node_id)))

#define SCMI_TRANSPORT_GET_RX_CHAN(node_id, proto)			\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 1),		\
		    (&(SCMI_TRANSPORT_CHAN_NAME(proto, 1))),		\
		    (_SCMI_TRANSPORT_GET_RX_BASE_CHAN(node_id)))

#define _SCMI_TRANSPORT_TX_CHAN_NAME(node_id, proto)			\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 0),		\
		    (SCMI_TRANSPORT_CHAN_NAME(proto, 0)),		\
		    (SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 0)))	\

#define SCMI_TRANSPORT_TX_CHAN_DECLARE_EXT(node_id, proto)\
	extern struct scmi_channel _SCMI_TRANSPORT_TX_CHAN_NAME(node_id, proto);

#define _SCMI_TRANSPORT_RX_CHAN_DECLARE_EXT_OPTIONAL(node_id, proto)			\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 1),				\
		    (extern struct scmi_channel SCMI_TRANSPORT_CHAN_NAME(proto, 1);),	\
		    ())

#define SCMI_TRANSPORT_RX_CHAN_DECLARE_EXT(node_id, proto)				\
	COND_CODE_1(_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 1),				\
		    (_SCMI_TRANSPORT_RX_CHAN_DECLARE_EXT(node_id, proto)),		\
		    (_SCMI_TRANSPORT_RX_CHAN_DECLARE_EXT_OPTIONAL(DT_PARENT(node_id),	\
								  SCMI_PROTOCOL_BASE)))

struct scmi_transport_api {
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
