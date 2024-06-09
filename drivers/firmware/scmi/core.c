/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/firmware/scmi/protocol.h>
#include <zephyr/drivers/firmware/scmi/transport.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(scmi_core);

/* note: this can be increased if need be */
/* TODO: maybe turn this into a configuration? */
#define SCMI_CHAN_LOCK_TIMEOUT_USEC 2000

/* TODO: maybe turn this into a configuration? */
#define SCMI_CHAN_SEM_TIMEOUT_USEC 1000

struct scmi_core_data {
	const struct device *transport;
};

static void scmi_core_reply_cb(struct scmi_channel *chan)
{
	if (!k_sem_count_get(&chan->sem)) {
		LOG_WRN("received unexpected reply");
		return;
	}

	k_sem_give(&chan->sem);

	return;
}

static void scmi_core_notification_cb(struct scmi_channel *chan)
{
	return;
}

int scmi_core_setup_chan(const struct device *core,
			 struct scmi_channel *chan,
			 bool tx)
{
	struct scmi_core_data *data;
	int ret;

	data = core->data;

	if (!chan) {
		return -EINVAL;
	}

	if (chan->ready) {
		return 0;
	}

	/* setup core-related channel data */
	k_mutex_init(&chan->lock);
	k_sem_init(&chan->sem, 0, 1);

	if (tx) {
		chan->cb = scmi_core_reply_cb;
	} else {
		chan->cb = scmi_core_notification_cb;
	}

	/* setup transport-related channel data */
	ret = scmi_transport_setup_chan(data->transport, chan, tx);
	if (ret < 0) {
		LOG_ERR("failed to setup channel");
		return ret;
	}

	chan->ready = true;

	return 0;
}

int scmi_core_send_message(struct scmi_protocol *proto,
			   struct scmi_message *msg,
			   struct scmi_message *reply)
{
	int ret;
	struct scmi_core_data *data;

	data = proto->core->data;
	ret = 0;

	if (!proto->tx) {
		return -ENODEV;
	}

	/* wait for channel to be free */
	ret = k_mutex_lock(&proto->tx->lock,
			   K_USEC(SCMI_CHAN_LOCK_TIMEOUT_USEC));
	if (ret < 0) {
		LOG_ERR("failed to acquire chan lock");
		return ret;
	}

	ret = scmi_transport_send_message(data->transport, proto->tx, msg);
	if (ret < 0) {
		LOG_ERR("failed to send message");
		goto out_release_mutex;
	}

	/* only one protocol instance can wait for a message reply at a time */
	ret = k_sem_take(&proto->tx->sem, K_USEC(SCMI_CHAN_SEM_TIMEOUT_USEC));
	if (ret < 0) {
		LOG_ERR("failed to wait for msg reply");
		goto out_release_mutex;
	}

	ret = scmi_transport_read_message(data->transport, proto->tx, reply);
	if (ret < 0) {
		LOG_ERR("failed to read reply");
		goto out_release_mutex;
	}

out_release_mutex:
	k_mutex_unlock(&proto->tx->lock);

	return ret;
}
