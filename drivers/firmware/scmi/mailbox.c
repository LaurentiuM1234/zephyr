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

	if (scmi_chan->cb)
		scmi_chan->cb(scmi_chan);
}

static int scmi_mbox_setup_chan(const struct device *transport,
				struct scmi_channel *chan,
				bool tx)
{
	int ret;
	struct scmi_mbox_channel *mbox_chan = chan->priv;

	if (tx) {
		if (mbox_chan->a2p_reply.dev) {
			ret = mbox_register_callback_dt(&mbox_chan->a2p_reply,
							scmi_mbox_cb, chan);
			if (ret < 0) {
				LOG_ERR("failed to register a2p reply cb");
				return ret;
			}

			ret = mbox_set_enabled_dt(&mbox_chan->a2p_reply, true);
			if (ret < 0) {
				LOG_ERR("failed to enable a2p reply dbell");
				return ret;
			}
		}

		ret = mbox_set_enabled_dt(&mbox_chan->a2p, true);
		if (ret < 0) {
			LOG_ERR("failed to enable a2p dbell");
			return ret;
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

	return 0;
}

struct scmi_transport_api scmi_mbox_api = {
	.setup_chan = scmi_mbox_setup_chan,
};

static int scmi_mbox_init(const struct device *transport)
{
	return 0;
}

SCMI_MAILBOX_INST_DEFINE(0, &scmi_mbox_init, &scmi_mbox_api);
