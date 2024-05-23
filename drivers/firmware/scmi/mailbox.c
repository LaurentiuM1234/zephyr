/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include "mailbox.h"

static int scmi_mailbox_send_message(const struct device *dev,
				     struct scmi_message *msg,
				     struct scmi_message *reply)
{
	const struct scmi_mailbox_config *cfg;
	struct scmi_mailbox_data *data;
	int ret;

	cfg = dev->config;
	data = dev->data;


	if (k_is_pre_kernel()) {
		/* TODO: add support for this */
		return -ENOTSUP;
	}

	/* IMPORTANT: the only way the channel could be busy would be
	 * in the case of ASYNC messages. Since there's no SMP and no
	 * ASYNC API in PRE-KERNEL that means there's no way the channel
	 * could be busy.
	 */
	__ASSERT(!scmi_shmem_is_busy(SCMI_TRANSPORT_CHAN_SHMEM(cfg, 1)),
		 "TX channel busy in PRE_KERNEL");

	data->waiting_reply = true;

	ret = scmi_shmem_write_message(SCMI_TRANSPORT_CHAN_SHMEM(cfg, 1), msg);
	if (ret) {
		LOG_ERR("failed to write message to shmem");
		return ret;
	}

	/* send platform notification */
	ret = mbox_send_dt(&cfg->a2p, NULL);
	if (ret) {
		LOG_ERR("failed to ring platform doorbell");
		return ret;
	}

	/* TODO: is there really no better way of doing this PRE_KERNEL stage? */
	while (data->waiting_reply);

	data->waiting_reply = false;

	/* get platform reply */
	ret = scmi_shmem_read_message(SCMI_TRANSPORT_CHAN_SHMEM(cfg, 1), reply);
	if (ret) {
		LOG_ERR("failed to read message from shmem");
		return ret;
	}

	/* TODO: do we need to check channel error status? */

	return 0;
}

static void scmi_mailbox_rx(const struct device *mbox_dev,
			    mbox_channel_id_t chan,
			    void *user_data, struct mbox_msg *data)
{
	const struct device *scmi_dev;
	const struct scmi_mailbox_config *scmi_cfg;
	struct scmi_mailbox_data *scmi_data;

	scmi_dev = user_data;
	scmi_cfg = scmi_dev->config;
	scmi_data = scmi_dev->data;

	if (chan == scmi_data->a2p_reply->channel_id) {
		scmi_data->waiting_reply = false;

		/* TODO: do we really need this? */
		compiler_barrier();

		return;
	}

	if (scmi_data->p2a_notification &&
	    chan == scmi_data->p2a_notification->channel_id) {
		/* TODO: platform notification */
		return;
	}

	LOG_ERR("dbell %d rang unexpectedly", chan);
}

static int scmi_mailbox_validate_configuration(const struct device *dev,
					       int shmem_num, int dbell_num)
{
	const struct scmi_mailbox_config *cfg = dev->config;

	if (dbell_num == 1 && shmem_num == 1) {
		return 0;
	} else if (dbell_num == 2 && shmem_num == 2) {
		if (cfg->a2p_reply.dev || !cfg->a2p.dev || !cfg->p2a.dev) {
			return -EINVAL;
		}
	} else if (dbell_num == 2 && shmem_num == 1) {
		if (cfg->p2a.dev || !cfg->a2p.dev || !cfg->a2p_reply.dev) {
			return -EINVAL;
		}
	} else if (dbell_num == 3 && shmem_num == 2) {
		if (!cfg->a2p.dev || !cfg->a2p_reply.dev || !cfg->p2a.dev) {
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static int scmi_mailbox_channel_prepare(const struct device *dev,
					int shmem_num, int dbell_num, bool tx)
{
	const struct scmi_mailbox_config *cfg;
	struct scmi_mailbox_data *data;
	struct mbox_dt_spec *a2p_reply, *p2a_notification;
	int ret;

	cfg = dev->config;
	data = dev->data;
	a2p_reply = NULL;
	p2a_notification = NULL;

	ret = scmi_mailbox_get_chan_dbells(dev, shmem_num, dbell_num,
					   a2p_reply, p2a_notification);
	if (ret < 0) {
		return ret;
	}

	data->a2p_reply = a2p_reply;
	data->p2a_notification = p2a_notification;

	if (tx) {
		/* TX channel is mandatory */
		if (!a2p_reply) {
			return -EINVAL;
		}

		ret = scmi_mailbox_dbell_prepare(dev, a2p_reply);
		if (ret) {
			return ret;
		}

		scmi_shmem_update_channel_flags(SCMI_TRANSPORT_CHAN_SHMEM(cfg, tx),
						SCMI_SHMEM_CHANNEL_FLAG_INT_EN_MASK, 0);
	} else {
		/* RX channel is optional */
		if (p2a_notification) {
			ret = scmi_mailbox_dbell_prepare(dev, p2a_notification);
			if (ret) {
				return ret;
			}
		}
	}

	return 0;
}

static struct scmi_transport_api scmi_mailbox_api = {
	.send_message = scmi_mailbox_send_message,
};

static int scmi_mailbox_init(const struct device *dev)
{
	const struct scmi_mailbox_config *cfg;
	struct scmi_mailbox_data *data;
	int ret, shmem_num, dbell_num;

	cfg = dev->config;
	data = dev->data;
	shmem_num = SCMI_TRANSPORT_CHAN_SHMEM(cfg, 0) == NULL ? 1 : 2;
	dbell_num = scmi_mailbox_dbell_count(dev);

	ret = scmi_mailbox_validate_configuration(dev, shmem_num, dbell_num);
	if (ret < 0) {
		return ret;
	}

	ret = scmi_mailbox_channel_prepare(dev, shmem_num, dbell_num, true);
	if (ret < 0) {
		return ret;
	}

	if (SCMI_TRANSPORT_CHAN_SHMEM(cfg, 0)) {
		ret = scmi_mailbox_channel_prepare(dev, shmem_num, dbell_num, false);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

/* TODO: don't forget to figure out the priority issue. Currently it's
 * hardcoded.
 */

#define SCMI_MAILBOX_INIT(inst)						\
									\
BUILD_ASSERT(DT_INST_PROP_LEN(inst, shmem) == 1 ||			\
	     DT_INST_PROP_LEN(inst, shmem) == 2,			\
	     "expected 1 or 2 shmem regions");				\
									\
BUILD_ASSERT(DT_INST_PROP_LEN(inst, mboxes) >= 1 &&			\
	     DT_INST_PROP_LEN(inst, mboxes) <= 4,			\
	     "mbox number should be in [1, 4]");			\
									\
BUILD_ASSERT((SCMI_TRANSPORT_SHMEM_NUM(inst) == 1 &&			\
	     SCMI_TRANSPORT_DBELL_NUM(inst) == 1) ||			\
	     (SCMI_TRANSPORT_SHMEM_NUM(inst) == 2 &&			\
	     SCMI_TRANSPORT_DBELL_NUM(inst) == 2) ||			\
	     (SCMI_TRANSPORT_SHMEM_NUM(inst) == 1 &&			\
	     SCMI_TRANSPORT_DBELL_NUM(inst) == 2) ||			\
	     (SCMI_TRANSPORT_SHMEM_NUM(inst) == 2 &&			\
	     SCMI_TRANSPORT_DBELL_NUM(inst) == 3),			\
	     "bad mbox and shmem count");				\
									\
BUILD_ASSERT(DT_INST_PROP_HAS_NAME(inst, mboxes, a2p),			\
	     "A2P mbox is mandatory");					\
									\
static struct scmi_mailbox_config config_##inst = {			\
	SCMI_TRANSPORT_PROLOGUE(inst),					\
	.a2p = SCMI_MAILBOX_DBELL(inst, a2p),				\
	.a2p_reply = SCMI_MAILBOX_DBELL(inst, a2p_reply),		\
	.p2a = SCMI_MAILBOX_DBELL(inst, p2a),				\
};									\
									\
static struct scmi_mailbox_data data_##inst;				\
									\
DEVICE_DT_INST_DEFINE(inst, &scmi_mailbox_init, NULL,			\
		      &data_##inst, &config_##inst, POST_KERNEL, 41,	\
		      &scmi_mailbox_api);				\

DT_INST_FOREACH_STATUS_OKAY(SCMI_MAILBOX_INIT);
