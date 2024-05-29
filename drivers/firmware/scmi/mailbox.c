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

static int _scmi_mailbox_send_message(struct scmi_channel *chan,
				      struct scmi_message *msg)
{
	int ret;

	if (chan->type != SCMI_CHANNEL_TX) {
		return -EINVAL;
	}

	/* TODO: should check if channel is busy */

	ret = scmi_shmem_write_message(chan->shmem, msg);
	if (ret < 0) {
		LOG_ERR("failed to write message to shmem");
		return ret;
	}

	ret = mbox_send_dt(&TO_TX_CHAN(chan)->a2p, NULL);
	if (ret < 0) {
		LOG_ERR("failed to ring doorbell");
		return ret;
	}

	return 0;
}

static void scmi_mailbox_rx(const struct device *mbox_dev,
			    mbox_channel_id_t mbox_chan,
			    void *user_data, struct mbox_msg *data)
{
	struct scmi_channel *chan = user_data;
}

static int scmi_mailbox_prepare_channel(struct scmi_channel *chan)
{
	int ret;
	struct mbox_dt_spec *a2p, *a2p_reply, *p2a;

	a2p = NULL;
	a2p_reply = NULL;
	p2a = NULL;

	switch (chan->type) {
	case SCMI_CHANNEL_TX:
		if (TO_TX_CHAN(chan)->a2p.dev) {
			a2p = &TO_TX_CHAN(chan)->a2p;
		}

		if (TO_TX_CHAN(chan)->a2p_reply.dev) {
			a2p_reply = &TO_TX_CHAN(chan)->a2p_reply;
		}
	case SCMI_CHANNEL_RX:
		if (TO_RX_CHAN(chan)->p2a.dev) {
			p2a = &TO_RX_CHAN(chan)->p2a;
		}
	default:
		return -EINVAL;
	}

	ret = scmi_mailbox_dbell_prepare(a2p, chan);
	if (ret < 0) {
		return ret;
	}

	ret = scmi_mailbox_dbell_prepare(a2p_reply, chan);
	if (ret < 0) {
		return ret;
	}

	ret = scmi_mailbox_dbell_prepare(p2a, chan);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static int scmi_mailbox_request_channel(const struct device *dev, int type,
					struct scmi_channel *chan)
{
	struct scmi_mailbox_data *data;
	int ret;

	data = dev->data;

	if (type != SCMI_CHANNEL_TX || type != SCMI_CHANNEL_RX) {
		return -EINVAL;
	}

	if (type == SCMI_CHANNEL_TX) {
		if (data->tx.header.valid) {
			chan = TO_SCMI_CHAN(&data->tx);
			return 0;
		}

		ret = scmi_mailbox_prepare_channel(TO_SCMI_CHAN(&data->tx));
		if (ret < 0) {
			return ret;
		}

		chan = TO_SCMI_CHAN(&data->tx);
	} else {
		if (!data->rx.p2a.dev) {
			return -ENODEV;
		}

		if (data->rx.header.valid) {
			chan = TO_SCMI_CHAN(&data->rx);
			return 0;
		}

		ret = scmi_mailbox_prepare_channel(TO_SCMI_CHAN(&data->rx));
		if (ret < 0) {
			return ret;
		}

		chan = TO_SCMI_CHAN(&data->rx);
	}

	return 0;
}

static struct scmi_transport_api scmi_mailbox_api = {
	.send_message = _scmi_mailbox_send_message,
	.request_channel = scmi_mailbox_request_channel,
};

static int scmi_mailbox_init(const struct device *dev)
{
	const struct scmi_mailbox_config *cfg;
	struct scmi_mailbox_data *data;

	cfg = dev->config;
	data = dev->data;


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
	     "A2P dbell is mandatory");					\
									\
BUILD_ASSERT(SCMI_TRANSPORT_SHMEM_NUM(inst) != 2 ||			\
	     SCMI_TRANSPORT_DBELL_NUM(inst) != 2 ||			\
	     DT_INST_PROP_HAS_NAME(inst, mboxes, p2a),			\
	     "no P2A dbell in bidirectional TX/RX configuration");	\
									\
BUILD_ASSERT(SCMI_TRANSPORT_SHMEM_NUM(inst) != 1 ||			\
	     SCMI_TRANSPORT_DBELL_NUM(inst) != 2 ||			\
	     DT_INST_PROP_HAS_NAME(inst, mboxes, a2p_reply),		\
	     "no A2P reply dbell in unidirectional TX configuration");	\
									\
BUILD_ASSERT(SCMI_TRANSPORT_SHMEM_NUM(inst) != 2 ||			\
	     SCMI_TRANSPORT_DBELL_NUM(inst) != 3 ||			\
	     (DT_INST_PROP_HAS_NAME(inst, mboxes, p2a) &&		\
	     DT_INST_PROP_HAS_NAME(inst, mboxes, a2p_reply)),		\
	     "no P2A / A2P reply dbell in unidirectional TX/RX configuration");\
									\
static struct scmi_mailbox_config config_##inst;			\
									\
static struct scmi_mailbox_data data_##inst = {				\
	.tx = SCMI_MAILBOX_TX_CHANNEL(inst),				\
	.rx = SCMI_MAILBOX_RX_CHANNEL(inst),				\
};									\
									\
DEVICE_DT_INST_DEFINE(inst, &scmi_mailbox_init, NULL,			\
		      &data_##inst, &config_##inst, POST_KERNEL, 41,	\
		      &scmi_mailbox_api);				\

DT_INST_FOREACH_STATUS_OKAY(SCMI_MAILBOX_INIT);
