/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_
#define _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_

#include <zephyr/drivers/firmware/scmi/transport.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(arm_scmi_mailbox);

#define DT_DRV_COMPAT arm_scmi

#define SCMI_MAILBOX_DBELL(inst, name)				\
	COND_CODE_1(DT_INST_PROP_HAS_NAME(inst, mboxes, name),	\
		    (MBOX_DT_SPEC_INST_GET(inst, name)),	\
		    ({ }))

#define SCMI_MAILBOX_TX_CHANNEL(inst)				\
{								\
	.header.dev = DEVICE_DT_INST_GET(inst),			\
	.header.shmem = DT_INST_SCMI_SHMEM_BY_IDX(inst, 0),	\
	.header.type = SCMI_CHANNEL_TX,				\
	.a2p = SCMI_MAILBOX_DBELL(inst, a2p),			\
	.a2p_reply = SCMI_MAILBOX_DBELL(inst, a2p_reply),	\
}

#define SCMI_MAILBOX_RX_CHANNEL(inst)				\
{								\
	.header.dev = DEVICE_DT_INST_GET(inst),			\
	.header.shmem = DT_INST_SCMI_SHMEM_BY_IDX(inst, 1),	\
	.header.type = SCMI_CHANNEL_RX,				\
	.p2a = SCMI_MAILBOX_DBELL(inst, p2a),			\
}

#define TO_SCMI_CHAN(tx_rx_chan) ((struct scmi_channel *)tx_rx_chan)
#define TO_TX_CHAN(scmi_chan) ((struct scmi_tx_channel *)scmi_chan)
#define TO_RX_CHAN(scmi_chan) ((struct scmi_rx_channel *)scmi_chan)

struct scmi_tx_channel {
	struct scmi_channel header;
	struct mbox_dt_spec a2p;
	struct mbox_dt_spec a2p_reply;
};

struct scmi_rx_channel {
	struct scmi_channel header;
	struct mbox_dt_spec p2a;
};

struct scmi_mailbox_config {
	SCMI_TRANSPORT_SHMEM_INFO;
	struct mbox_dt_spec a2p;
	struct mbox_dt_spec a2p_reply;
	struct mbox_dt_spec p2a;
};

struct scmi_mailbox_data {
	const struct mbox_dt_spec *a2p_reply;
	const struct mbox_dt_spec *p2a_notification;
	volatile bool waiting_reply;
	struct scmi_tx_channel tx;
	struct scmi_rx_channel rx;
};

static void scmi_mailbox_rx(const struct device *mbox_dev,
			    mbox_channel_id_t chan, void *user_data,
			    struct mbox_msg *data);

static inline int scmi_mailbox_dbell_count(const struct device *dev)
{
	const struct scmi_mailbox_config *cfg;
	int num;

	cfg = dev->config;
	num = 0;

	num += (cfg->a2p.dev ? 1 : 0);
	num += (cfg->a2p_reply.dev ? 1 : 0);
	num += (cfg->p2a.dev ? 1 : 0);

	return num;
}

static inline int scmi_mailbox_dbell_prepare(struct mbox_dt_spec *dbell, void *cb_data)
{
	int ret;

	if (!dbell)
		return 0;

	ret = mbox_set_enabled_dt(dbell, true);
	if (ret < 0) {
		LOG_ERR("failed to enable channel %d", dbell->channel_id);
		return ret;
	}

	return mbox_register_callback_dt(dbell, scmi_mailbox_rx, cb_data);
}

static inline int scmi_mailbox_get_chan_dbells(const struct device *dev,
					       int shmem_num, int dbell_num,
					       const struct mbox_dt_spec *a2p_reply,
					       const struct mbox_dt_spec *p2a_notification)
{
	const struct scmi_mailbox_config *cfg;

	cfg = dev->config;
	a2p_reply = NULL;
	p2a_notification = NULL;

	if (dbell_num == 1 && shmem_num == 1) {
		a2p_reply = &cfg->a2p;
	} else if (dbell_num == 2 && shmem_num == 2) {
		a2p_reply = &cfg->a2p;
		p2a_notification = &cfg->p2a;
	} else if (dbell_num == 2 && shmem_num == 1) {
		a2p_reply = &cfg->a2p_reply;
	} else {
		return -EINVAL;
	}

	return 0;
}

#endif /* _ZEPHYR_DRIVERS_FIRMWARE_SCMI_MAILBOX_H_ */
