/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_UTIL_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_UTIL_H_

#ifdef CONFIG_ARM_SCMI_TRANSPORT_HAS_STATIC_CHANNELS

#ifdef CONFIG_ARM_SCMI_MAILBOX_TRANSPORT
#define DT_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, idx)\
	DT_PROP_HAS_IDX(node_id, shmem, idx)
#else /* CONFIG_ARM_SCMI_MAILBOX_TRANSPORT */
#error "Transport with static channels needs to define HAS_CHAN macro"
#endif /* CONFIG_ARM_SCMI_MAILBOX_TRANSPORT */

/* utility macros */
#define SCMI_TRANSPORT_CHAN_NAME(proto, idx) CONCAT(scmi_channel_, proto, _, idx)
#define SCMI_PROTOCOL_NAME(proto) CONCAT(scmi_protocol_, proto)

/* declare a transport channel using the extern qualifier */
#define DT_SCMI_TRANSPORT_TX_CHAN_DECLARE(node_id)				\
	COND_CODE_1(DT_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 0),		\
		    (extern struct scmi_channel					\
		     SCMI_TRANSPORT_CHAN_NAME(DT_REG_ADDR(node_id), 0);),	\
		    (extern struct scmi_channel					\
		     SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 0);))		\

#define DT_SCMI_TRANSPORT_RX_BASE_CHAN_DECLARE(node_id)				\
	COND_CODE_1(DT_SCMI_TRANSPORT_PROTO_HAS_CHAN(DT_PARENT(node_id), 1),	\
		    (extern struct scmi_channel					\
		     SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 1);),		\
		    ())

#define DT_SCMI_TRANSPORT_RX_CHAN_DECLARE(node_id)				\
	COND_CODE_1(DT_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 1),		\
		    (extern struct scmi_channel					\
		     SCMI_TRANSPORT_CHAN_NAME(DT_REG_ADDR(node_id), 1);),	\
		    (DT_SCMI_TRANSPORT_RX_BASE_CHAN_DECLARE(node_id)))

#define DT_SCMI_TRANSPORT_CHANNELS_DECLARE(node_id)				\
	DT_SCMI_TRANSPORT_TX_CHAN_DECLARE(node_id)				\
	DT_SCMI_TRANSPORT_RX_CHAN_DECLARE(node_id)

#define DT_INST_SCMI_TRANSPORT_CHANNELS_DECLARE(inst)				\
	DT_SCMI_TRANSPORT_CHANNELS_DECLARE(DT_INST(inst, DT_DRV_COMPAT))

/* macros used to get reference to a channel */
#define DT_SCMI_TRANSPORT_TX_CHAN(node_id)					\
	COND_CODE_1(DT_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 0),		\
		    (&SCMI_TRANSPORT_CHAN_NAME(DT_REG_ADDR(node_id), 0)),	\
		    (&SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 0)))

#define DT_SCMI_TRANSPORT_RX_BASE_CHAN(node_id)					\
	COND_CODE_1(DT_SCMI_TRANSPORT_PROTO_HAS_CHAN(DT_PARENT(node_id), 1),	\
		    (&SCMI_TRANSPORT_CHAN_NAME(SCMI_PROTOCOL_BASE, 1)),		\
		    (NULL))

#define DT_SCMI_TRANSPORT_RX_CHAN(node_id)					\
	COND_CODE_1(DT_SCMI_TRANSPORT_PROTO_HAS_CHAN(node_id, 1),		\
		    (&SCMI_TRANSPORT_CHAN_NAME(DT_REG_ADDR(node_id), 1)),	\
		    (DT_SCMI_TRANSPORT_RX_BASE_CHAN(node_id)))

#define DT_SCMI_TRANSPORT_CHAN_DEFINE(node_id, idx, proto, data)		\
	STRUCT_SECTION_ITERABLE(scmi_channel,					\
				SCMI_TRANSPORT_CHAN_NAME(proto, idx)) =		\
	{									\
		.priv = data,							\
	}

#define DT_SCMI_PROTOCOL_DATA_DEFINE(node_id, proto, data)			\
	STRUCT_SECTION_ITERABLE(scmi_protocol, SCMI_PROTOCOL_NAME(proto)) =	\
	{									\
		.id = proto,							\
		.tx = DT_SCMI_TRANSPORT_TX_CHAN(node_id),			\
		.rx = DT_SCMI_TRANSPORT_RX_CHAN(node_id),			\
		.priv = data,							\
	}

#else /* CONFIG_ARM_SCMI_TRANSPORT_HAS_STATIC_CHANNELS */

#define DT_SCMI_TRANSPORT_CHANNELS_DECLARE(node_id)

#define DT_SCMI_PROTOCOL_DATA_DEFINE(node_id, proto, data)			\
	STRUCT_SECTION_ITERABLE(scmi_protocol, SCMI_PROTOCOL_NAME(proto)) =	\
	{									\
		.id = proto,							\
		.priv = data,							\
	}

#endif /* CONFIG_ARM_SCMI_TRANSPORT_HAS_STATIC_CHANNELS */

#define DT_INST_SCMI_TRANSPORT_DEFINE(inst, pm, data, config, level, prio, api)	\
	DEVICE_DT_INST_DEFINE(inst, &scmi_core_transport_init,			\
			      pm, data, config, level, prio, api)

#define DT_SCMI_PROTOCOL_DEFINE(node_id, init_fn, pm, data, config,		\
				level, prio, api)				\
	DT_SCMI_TRANSPORT_CHANNELS_DECLARE(node_id)				\
	DT_SCMI_PROTOCOL_DATA_DEFINE(node_id, DT_REG_ADDR(node_id), data);	\
	DEVICE_DT_DEFINE(node_id, init_fn, pm, data, config, level, prio, api)	\

#define DT_INST_SCMI_PROTOCOL_DEFINE(inst, init_fn, pm, data, config,		\
				     level, prio, api)				\
	DT_SCMI_PROTOCOL_DEFINE(DT_INST(inst, DT_DRV_COMPAT), init_fn, pm,	\
				data, config, level, prio, api)

#define DT_SCMI_PROTOCOL_DEFINE_NODEV(node_id, data)				\
	DT_SCMI_TRANSPORT_CHANNELS_DECLARE(node_id)				\
	DT_SCMI_PROTOCOL_DATA_DEFINE(node_id, DT_REG_ADDR(node_id), data)


#define SCMI_FIELD_MAKE(x, mask, shift)\
	(((uint32_t)(x) & (mask)) << (shift))

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_UTIL_H_ */
