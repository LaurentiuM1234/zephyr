/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_
#define _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_

#include <zephyr/drivers/firmware/scmi/transport.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/kernel.h>

#define DT_DRV_COMPAT arm_scmi

#define _SCMI_MBOX_CHAN_NAME(proto, tx)\
	CONCAT(SCMI_TRANSPORT_CHAN_NAME(proto, tx), _, priv)

#define _SCMI_MBOX_CHAN_DBELL(node_id, name)			\
	COND_CODE_1(DT_PROP_HAS_NAME(node_id, mboxes, name),	\
		    (MBOX_DT_SPEC_GET(node_id, name)),		\
		    ({ }))

#define _SCMI_MBOX_CHAN_DECLARE_PRIV_TX(node_id, proto)		\
{								\
	.a2p = MBOX_DT_SPEC_GET(node_id, a2p),			\
	.a2p_reply = _SCMI_MBOX_CHAN_DBELL(node_id, a2p_reply),	\
}

#define _SCMI_MBOX_CHAN_DECLARE_PRIV_RX(node_id, proto)	\
{							\
	.p2a = MBOX_DT_SPEC_GET(node_id, p2a),		\
}

#define _SCMI_MBOX_CHAN_DECLARE_PRIV_TX_RX(node_id, proto, tx)		\
	COND_CODE_1(tx,							\
		    (_SCMI_MBOX_CHAN_DECLARE_PRIV_TX(node_id, proto)),	\
		    (_SCMI_MBOX_CHAN_DECLARE_PRIV_RX(node_id, proto)))

#define _SCMI_MBOX_CHAN_DECLARE_PRIV(node_id, proto, tx)			\
	static struct scmi_mbox_channel						\
		    _SCMI_MBOX_CHAN_NAME(proto, tx) =				\
		    _SCMI_MBOX_CHAN_DECLARE_PRIV_TX_RX(node_id, proto, tx)


#define _SCMI_MBOX_CHAN_DECLARE(node_id, proto, tx)			\
	_SCMI_MBOX_CHAN_DECLARE_PRIV(node_id, proto, tx);		\
	SCMI_TRANSPORT_CHAN_DECLARE(node_id, proto, tx,			\
				    &(_SCMI_MBOX_CHAN_NAME(proto, tx)));\

#define _SCMI_MBOX_CHAN_DECLARE_OPTIONAL(node_id, proto, tx)		\
	COND_CODE_1(DT_PROP_HAS_IDX(node_id, shmem, tx),		\
		    (_SCMI_MBOX_CHAN_DECLARE(node_id, proto, tx)),	\
		    ())

#define SCMI_MBOX_PROTO_CHAN_DECLARE(node_id)					\
	_SCMI_MBOX_CHAN_DECLARE_OPTIONAL(node_id, DT_REG_ADDR(node_id), 0)	\
	_SCMI_MBOX_CHAN_DECLARE_OPTIONAL(node_id, DT_REG_ADDR(node_id), 1)

#define DT_INST_SCMI_MBOX_BASE_CHAN_DECLARE(inst)			\
	_SCMI_MBOX_CHAN_DECLARE_OPTIONAL(DT_INST(inst, DT_DRV_COMPAT),	\
					 SCMI_PROTOCOL_BASE, 0)		\
	_SCMI_MBOX_CHAN_DECLARE_OPTIONAL(DT_INST(inst, DT_DRV_COMPAT),	\
					 SCMI_PROTOCOL_BASE, 1)

#define SCMI_MAILBOX_INST_DEFINE(inst, init, api)				\
	DT_INST_FOREACH_CHILD_STATUS_OKAY(inst, SCMI_MBOX_PROTO_CHAN_DECLARE)	\
	DT_INST_SCMI_MBOX_BASE_CHAN_DECLARE(inst)				\
	DEVICE_DT_INST_DEFINE(inst, init, NULL, NULL, NULL,			\
			      POST_KERNEL,					\
			      CONFIG_ARM_SCMI_TRANSPORT_INIT_PRIORITY,		\
			      api)

struct scmi_mbox_channel {
	const struct device *shmem;
	struct mbox_dt_spec a2p;
	struct mbox_dt_spec p2a;
	struct mbox_dt_spec a2p_reply;
};

#endif /* _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_ */
