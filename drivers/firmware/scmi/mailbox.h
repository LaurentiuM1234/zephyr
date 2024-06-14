/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_
#define _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_

#include <zephyr/drivers/firmware/scmi/transport.h>
#include <zephyr/drivers/firmware/scmi/shmem.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/kernel.h>

#define DT_DRV_COMPAT arm_scmi

#define _SCMI_MBOX_SHMEM_BY_IDX(node_id, idx)					\
	COND_CODE_1(DT_PROP_HAS_IDX(node_id, shmem, idx),			\
		    (DEVICE_DT_GET(DT_PROP_BY_IDX(node_id, shmem, idx))),	\
		    (NULL))

#define _SCMI_MBOX_CHAN_NAME(proto, idx)\
	CONCAT(SCMI_TRANSPORT_CHAN_NAME(proto, idx), _, priv)

#define _SCMI_MBOX_CHAN_DBELL(node_id, name)			\
	COND_CODE_1(DT_PROP_HAS_NAME(node_id, mboxes, name),	\
		    (MBOX_DT_SPEC_GET(node_id, name)),		\
		    ({ }))

#define _SCMI_MBOX_CHAN_DEFINE_PRIV_TX(node_id, proto)		\
{								\
	.shmem = _SCMI_MBOX_SHMEM_BY_IDX(node_id, 0),		\
	.tx = MBOX_DT_SPEC_GET(node_id, tx),			\
	.tx_reply = _SCMI_MBOX_CHAN_DBELL(node_id, tx_reply),	\
}

#define _SCMI_MBOX_CHAN_DEFINE_PRIV_RX(node_id, proto)	\
{							\
	.shmem = _SCMI_MBOX_SHMEM_BY_IDX(node_id, 1),	\
	.rx = MBOX_DT_SPEC_GET(node_id, rx),		\
}

#define _SCMI_MBOX_CHAN_DEFINE_PRIV_TX_RX(node_id, proto, idx)		\
	COND_CODE_1(idx,						\
		    (_SCMI_MBOX_CHAN_DEFINE_PRIV_RX(node_id, proto)),	\
		    (_SCMI_MBOX_CHAN_DEFINE_PRIV_TX(node_id, proto)))

#define _SCMI_MBOX_CHAN_DEFINE_PRIV(node_id, proto, idx)			\
	static struct scmi_mbox_channel						\
		    _SCMI_MBOX_CHAN_NAME(proto, idx) =				\
		    _SCMI_MBOX_CHAN_DEFINE_PRIV_TX_RX(node_id, proto, idx)


#define _SCMI_MBOX_CHAN_DEFINE(node_id, proto, idx)				\
	_SCMI_MBOX_CHAN_DEFINE_PRIV(node_id, proto, idx);			\
	SCMI_TRANSPORT_CHAN_DEFINE(node_id, proto, idx,				\
				    &(_SCMI_MBOX_CHAN_NAME(proto, idx)));	\

#define _SCMI_MBOX_CHAN_DEFINE_OPTIONAL(node_id, proto, idx)		\
	COND_CODE_1(DT_PROP_HAS_IDX(node_id, shmem, idx),		\
		    (_SCMI_MBOX_CHAN_DEFINE(node_id, proto, idx)),	\
		    ())

#define SCMI_MBOX_PROTO_CHAN_DEFINE(node_id)					\
	_SCMI_MBOX_CHAN_DEFINE_OPTIONAL(node_id, DT_REG_ADDR(node_id), 0)	\
	_SCMI_MBOX_CHAN_DEFINE_OPTIONAL(node_id, DT_REG_ADDR(node_id), 1)

#define DT_INST_SCMI_MBOX_BASE_CHAN_DEFINE(inst)			\
	_SCMI_MBOX_CHAN_DEFINE_OPTIONAL(DT_INST(inst, DT_DRV_COMPAT),	\
					 SCMI_PROTOCOL_BASE, 0)		\
	_SCMI_MBOX_CHAN_DEFINE_OPTIONAL(DT_INST(inst, DT_DRV_COMPAT),	\
					 SCMI_PROTOCOL_BASE, 1)

#define DT_INST_SCMI_MAILBOX_DEFINE(inst, level, prio, api)			\
	DT_INST_FOREACH_CHILD_STATUS_OKAY(inst, SCMI_MBOX_PROTO_CHAN_DEFINE)	\
	DT_INST_SCMI_MBOX_BASE_CHAN_DEFINE(inst)				\
	DT_INST_SCMI_TRANSPORT_DEFINE(inst, NULL, NULL, NULL, level, prio, api)

struct scmi_mbox_channel {
	const struct device *shmem;
	struct mbox_dt_spec tx;
	struct mbox_dt_spec rx;
	struct mbox_dt_spec tx_reply;
};

#endif /* _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_ */
