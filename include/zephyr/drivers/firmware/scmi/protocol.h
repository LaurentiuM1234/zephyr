/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_

#include <zephyr/device.h>
#include <zephyr/drivers/firmware/scmi/util.h>
#include <stdint.h>
#include <errno.h>

/* TODO: add note about this being in decimal */
#define SCMI_PROTOCOL_BASE 16
#define SCMI_PROTOCOL_POWER_DOMAIN 17
#define SCMI_PROTOCOL_SYSTEM 18
#define SCMI_PROTOCOL_PERF 19
#define SCMI_PROTOCOL_CLOCK 20
#define SCMI_PROTOCOL_SENSOR 21
#define SCMI_PROTOCOL_RESET_DOMAIN 22
#define SCMI_PROTOCOL_VOLTAGE_DOMAIN 23
#define SCMI_PROTOCOL_PCAP_MONITOR 24
#define SCMI_PROTOCOL_PINCTRL 25

#define SCMI_MESSAGE_HDR_MAKE(id, type, proto, token)	\
	(SCMI_FIELD_MAKE(id, GENMASK(7, 0), 0)     |	\
	 SCMI_FIELD_MAKE(type, GENMASK(1, 0), 8)   |	\
	 SCMI_FIELD_MAKE(proto, GENMASK(7, 0), 10) |	\
	 SCMI_FIELD_MAKE(token, GENMASK(9, 0), 18))

struct scmi_channel;

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

struct scmi_protocol {
	uint32_t id;
	struct scmi_channel *tx;
	struct scmi_channel *rx;
	const struct device *transport;
	void *priv;
};

struct scmi_message {
	uint32_t hdr;
	uint32_t len;
	void *content;
};

int scmi_status_to_linux(int scmi_status);
int scmi_send_message(struct scmi_protocol *proto,
		      struct scmi_message *msg, struct scmi_message *reply);

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_ */
