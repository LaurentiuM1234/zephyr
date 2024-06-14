/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_

#include <zephyr/device.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/drivers/firmware/scmi/util.h>

struct scmi_message;
struct scmi_channel;

typedef void (*scmi_channel_cb)(struct scmi_channel *chan);

struct scmi_channel {
	struct k_mutex lock;
	struct k_sem sem;
	void *priv;
	scmi_channel_cb cb;
	bool ready;
	volatile bool received_reply;
};

struct scmi_transport_api {
	int (*init)(const struct device *transport);
	int (*send_message)(const struct device *transport,
			    struct scmi_channel *chan,
			    struct scmi_message *msg);
	int (*setup_chan)(const struct device *transport,
			  struct scmi_channel *chan,
			  bool tx);
	int (*read_message)(const struct device *transport,
			    struct scmi_channel *chan,
			    struct scmi_message *msg);
};

int scmi_core_transport_init(const struct device *transport);

static inline int scmi_transport_init(const struct device *transport)
{
	const struct scmi_transport_api *api =
		(const struct scmi_transport_api *)transport->api;

	if (api->init) {
		return api->init(transport);
	}

	return 0;
}

static inline int scmi_transport_setup_chan(const struct device *transport,
					    struct scmi_channel *chan,
					    bool tx)
{
	const struct scmi_transport_api *api =
		(const struct scmi_transport_api *)transport->api;

	if (!api || !api->setup_chan) {
		return -ENOSYS;
	}

	return api->setup_chan(transport, chan, tx);
}

static inline int scmi_transport_send_message(const struct device *transport,
					      struct scmi_channel *chan,
					      struct scmi_message *msg)
{
	const struct scmi_transport_api *api =
		(const struct scmi_transport_api *)transport->api;

	if (!api || !api->send_message) {
		return -ENOSYS;
	}

	return api->send_message(transport, chan, msg);
}

static inline int scmi_transport_read_message(const struct device *transport,
					      struct scmi_channel *chan,
					      struct scmi_message *msg)
{
	const struct scmi_transport_api *api =
		(const struct scmi_transport_api *)transport->api;

	if (!api || !api->read_message) {
		return -ENOSYS;
	}

	return api->read_message(transport, chan, msg);
}

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_TRANSPORT_H_ */
