/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_COMMON_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_COMMON_H_

#include <zephyr/sys/mutex.h>

#define _SCMI_FIELD_MAKE(x, mask, shift)\
	(((uint32_t)(x) & (mask)) << (shift))

#define SCMI_MESSAGE_HDR_MAKE(id, type, proto, token)	\
	(_SCMI_FIELD_MAKE(id, GENMASK(7, 0), 0) |	\
	 _SCMI_FIELD_MAKE(type, GENMASK(1, 0), 8) |	\
	 _SCMI_FIELD_MAKE(proto, GENMASK(7, 0), 10) |	\
	 _SCMI_FIELD_MAKE(token, GENMASK(9, 0), 18))

struct scmi_channel;
struct scmi_protocol;

enum scmi_message_type {
	SCMI_COMMAND = 0x0,
	SCMI_DELAYED_REPLY = 0x2,
	SCMI_NOTIFICATION = 0x3,
};

enum scmi_status_code {
	SCMI_SUCCESS = 0,
	SCMI_NOT_SUPPORTED = -1,
	SCMI_INVALID_PARAMETERS = -2,
	SCMI_DENIED = -3,
	SCMI_NOT_FOUND = -4,
	SCMI_OUT_OF_RANGE = -5,
	SCMI_BUSY = -6,
	SCMI_COMMS_ERROR = -7,
	SCMI_GENERIC_ERROR = -8,
	SCMI_HARDWARE_ERROR = -9,
	SCMI_PROTOCOL_ERROR = -10,
	SCMI_IN_USE = -11,
};

struct scmi_message {
	uint32_t hdr;
	uint32_t len;
	void *content;
};

typedef void (*scmi_channel_cb)(struct scmi_channel *chan);

struct scmi_channel {
	struct k_mutex lock;
	struct k_sem sem;
	void *priv;
	scmi_channel_cb cb;
	bool ready;
	volatile bool received_reply;
};

static inline int scmi_ret_to_linux(int scmi_ret) {
	switch (scmi_ret) {
	case SCMI_SUCCESS:
		return 0;
	case SCMI_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	case SCMI_INVALID_PARAMETERS:
		return -EINVAL;
	case SCMI_DENIED:
		return -EACCES;
	case SCMI_NOT_FOUND:
		return -ENOENT;
	case SCMI_OUT_OF_RANGE:
		return -ERANGE;
	case SCMI_IN_USE:
	case SCMI_BUSY:
		return -EBUSY;
	case SCMI_PROTOCOL_ERROR:
		return -EPROTO;
	case SCMI_COMMS_ERROR:
	case SCMI_GENERIC_ERROR:
	case SCMI_HARDWARE_ERROR:
	default:
		return -EIO;
	}
}

int scmi_core_send_message(struct scmi_protocol *proto,
			   struct scmi_message *msg,
			   struct scmi_message *reply);
int scmi_core_transport_init(const struct device *transport);

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_COMMON_H_ */
