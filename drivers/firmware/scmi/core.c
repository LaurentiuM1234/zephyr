/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/firmware/scmi/protocol.h>
#include <zephyr/drivers/firmware/scmi/transport.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>

LOG_MODULE_REGISTER(scmi_core);

/* note: this can be increased if need be */
/* TODO: maybe turn this into a configuration? */
#define SCMI_CHAN_LOCK_TIMEOUT_USEC 2000

/* TODO: maybe turn this into a configuration? */
#define SCMI_CHAN_SEM_TIMEOUT_USEC 1000

static void scmi_core_reply_cb(struct scmi_channel *chan)
{
	if (k_is_pre_kernel()) {
		chan->received_reply = true;
	} else {
		k_sem_give(&chan->sem);
	}

	return;
}

static int scmi_core_setup_chan(const struct device *transport,
				struct scmi_channel *chan, bool tx)
{
	int ret;

	if (!chan) {
		return -EINVAL;
	}

	if (chan->ready) {
		return 0;
	}

	k_mutex_init(&chan->lock);
	k_sem_init(&chan->sem, 0, 1);

	chan->received_reply = false;

	if (tx) {
		chan->cb = scmi_core_reply_cb;
	}

	/* setup transport-related channel data */
	ret = scmi_transport_setup_chan(transport, chan, tx);
	if (ret < 0) {
		LOG_ERR("failed to setup channel");
		return ret;
	}

	chan->ready = true;

	return 0;
}

static int scmi_core_send_message_pre_kernel(struct scmi_protocol *proto,
					     struct scmi_message *msg,
					     struct scmi_message *reply)
{
	int ret;

	ret = scmi_transport_send_message(proto->transport, proto->tx, msg);
	if (ret < 0) {
		return ret;
	}

	while (!proto->tx->received_reply);

	proto->tx->received_reply = false;

	ret = scmi_transport_read_message(proto->transport, proto->tx, reply);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

static int scmi_core_send_message_post_kernel(struct scmi_protocol *proto,
					      struct scmi_message *msg,
					      struct scmi_message *reply)
{
	int ret;

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

	ret = scmi_transport_send_message(proto->transport, proto->tx, msg);
	if (ret < 0) {
		LOG_ERR("failed to send message");
		goto out_release_mutex;
	}

	/* only one protocol instance can wait for a message reply at a time */
	ret = k_sem_take(&proto->tx->sem, K_FOREVER);
	if (ret < 0) {
		LOG_ERR("failed to wait for msg reply");
		goto out_release_mutex;
	}

	ret = scmi_transport_read_message(proto->transport, proto->tx, reply);
	if (ret < 0) {
		LOG_ERR("failed to read reply");
		goto out_release_mutex;
	}

out_release_mutex:
	k_mutex_unlock(&proto->tx->lock);

	return ret;
}

int scmi_core_send_message(struct scmi_protocol *proto,
			   struct scmi_message *msg,
			   struct scmi_message *reply)
{
	if (!proto->tx) {
		return -ENODEV;
	}

	if (!proto->tx->ready) {
		return -EINVAL;
	}

	if (k_is_pre_kernel()) {
		return scmi_core_send_message_pre_kernel(proto, msg, reply);
	} else {
		return scmi_core_send_message_post_kernel(proto, msg, reply);
	}
}

static int scmi_core_protocol_setup(const struct device *transport)
{
	int ret;

	STRUCT_SECTION_FOREACH(scmi_protocol, it) {
		/* bind transport */
		it->transport = transport;

		/* TX channel is mandatory */
		if (!it->tx) {
			return -EINVAL;
		}

		ret = scmi_core_setup_chan(transport, it->tx, true);
		if (ret < 0) {
			return ret;
		}

		/* RX channel is optional */
		if (!it->rx) {
			continue;
		}

		ret = scmi_core_setup_chan(transport, it->rx, false);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

int scmi_core_transport_init(const struct device *transport)
{
	int ret;

	ret = scmi_transport_init(transport);
	if (ret < 0) {
		return ret;
	}

	return scmi_core_protocol_setup(transport);
}

