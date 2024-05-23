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

static inline int scmi_mailbox_dbell_prepare(const struct device *dev,
					     struct mbox_dt_spec *dbell)
{
	int ret = mbox_set_enabled_dt(dbell, true);
	if (ret < 0) {
		LOG_ERR("failed to enable channel %d", dbell->channel_id);
		return ret;
	}

	return mbox_register_callback_dt(dbell, scmi_mailbox_rx, (void *)dev);
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
