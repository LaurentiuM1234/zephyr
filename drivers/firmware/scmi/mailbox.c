/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include "mailbox.h"

LOG_MODULE_REGISTER(scmi_mbox);

static void scmi_mbox_cb(const struct device *mbox,
			 mbox_channel_id_t channel_id,
			 void *user_data,
			 struct mbox_msg *data)
{
	struct scmi_channel *scmi_chan = user_data;

	LOG_DBG("new message on channel %d", channel_id);

	if (scmi_chan->cb)
		scmi_chan->cb(scmi_chan);
}

static int scmi_mbox_send_message(const struct device *transport,
				  struct scmi_channel *chan,
				  struct scmi_message *msg)
{
	struct scmi_mbox_channel *mbox_chan;
	int ret;

	mbox_chan = chan->priv;

	ret = scmi_shmem_write_message(mbox_chan->shmem, msg);
	if (ret < 0) {
		LOG_ERR("failed to write message to shmem: %d", ret);
		return ret;
	}

	ret = mbox_send_dt(&mbox_chan->a2p, NULL);
	if (ret < 0) {
		LOG_ERR("failed to ring doorbell: %d", ret);
		return ret;
	}

	return 0;
}

static int scmi_mbox_read_message(const struct device *transport,
				  struct scmi_channel *chan,
				  struct scmi_message *msg)
{
	struct scmi_mbox_channel *mbox_chan;

	mbox_chan = chan->priv;

	return scmi_shmem_read_message(mbox_chan->shmem, msg);
}

static int scmi_mbox_setup_chan(const struct device *transport,
				struct scmi_channel *chan,
				bool tx)
{
	int ret;
	struct scmi_mbox_channel *mbox_chan;
	struct mbox_dt_spec *a2p_reply;

	mbox_chan = chan->priv;

	/* TODO: this needs to be validated elsewhere */
	if (tx && !mbox_chan->a2p.dev) {
		LOG_ERR("tx channel missing a2p dbell");
		return -EINVAL;
	}

	if (tx) {
		if (mbox_chan->a2p_reply.dev) {
			a2p_reply = &mbox_chan->a2p_reply;
		} else {
			a2p_reply = &mbox_chan->a2p;
		}

		ret = mbox_register_callback_dt(a2p_reply, scmi_mbox_cb, chan);
		if (ret < 0) {
			LOG_ERR("failed to register a2p reply cb");
			return ret;
		}

		ret = mbox_set_enabled_dt(a2p_reply, true);
		if (ret < 0) {
			LOG_ERR("failed to enable a2p reply dbell");
		}
	} else {
		if (mbox_chan->p2a.dev) {
			ret = mbox_register_callback_dt(&mbox_chan->p2a,
							scmi_mbox_cb, chan);
			if (ret < 0) {
				LOG_ERR("failed to register p2a cb");
				return ret;
			}

			ret = mbox_set_enabled_dt(&mbox_chan->p2a, true);
			if (ret < 0) {
				LOG_ERR("failed to enable p2a dbell");
				return ret;
			}
		}

	}

	/* enable interrupt-based communication */
	scmi_shmem_update_flags(mbox_chan->shmem,
				SCMI_SHMEM_CHAN_FLAG_IRQ_BIT,
				SCMI_SHMEM_CHAN_FLAG_IRQ_BIT);

	return 0;
}

struct scmi_transport_api scmi_mbox_api = {
	.setup_chan = scmi_mbox_setup_chan,
	.send_message = scmi_mbox_send_message,
	.read_message = scmi_mbox_read_message,
};

static int scmi_mbox_init(const struct device *transport)
{
	return 0;
}

SCMI_MAILBOX_INST_DEFINE(0, &scmi_mbox_init, &scmi_mbox_api);
