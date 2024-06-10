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

struct scmi_core {
	const struct device *transport;
};

static struct scmi_core core = {
#ifdef CONFIG_ARM_SCMI
	/* shmem + dbell-based transport */
	.transport = DEVICE_DT_GET(DT_INST(0, arm_scmi)),
#endif /* CONFIG_ARM_SCMI */
};

static void scmi_core_reply_cb(struct scmi_channel *chan)
{
	k_sem_give(&chan->sem);

	return;
}

static void scmi_core_notification_cb(struct scmi_channel *chan)
{
	return;
}

static int scmi_core_setup_chan(struct scmi_channel *chan, bool tx)
{
	int ret;

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
	ret = scmi_transport_setup_chan(core.transport, chan, tx);
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

	ret = scmi_transport_send_message(core.transport, proto->tx, msg);
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

	ret = scmi_transport_read_message(core.transport, proto->tx, reply);
	if (ret < 0) {
		LOG_ERR("failed to read reply");
		goto out_release_mutex;
	}

out_release_mutex:
	k_mutex_unlock(&proto->tx->lock);

	return ret;
}

static int scmi_core_init(void)
{
	int ret;

	if (!core.transport) {
		LOG_ERR("no transport registered to core");
		return -EINVAL;
	}

	STRUCT_SECTION_FOREACH(scmi_protocol, it) {
		/* sanity checks */
		/* TODO: for now, dynamic channel setup is not allowed */
		if (!it->tx) {
			LOG_ERR("protocol %d has invalid TX channel", it->id);
			return -EINVAL;
		}

		ret = scmi_core_setup_chan(it->tx, true);
		if (ret < 0) {
			LOG_ERR("failed to setup TX channel for %d", it->id);
			return ret;
		}

		/* RX channel is optional */
		if (!it->rx) {
			continue;
		}

		ret = scmi_core_setup_chan(it->rx, false);
		if (ret < 0) {
			LOG_ERR("failed to setup RX channel for %d", it->id);
			return ret;
		}
	}

	return 0;
}

SYS_INIT(scmi_core_init, POST_KERNEL, CONFIG_ARM_SCMI_CORE_INIT_PRIORITY);
