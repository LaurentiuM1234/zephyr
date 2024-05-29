/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_

#include <zephyr/device.h>

#define SCMI_PROTOCOL_ID(msg) ((GENMASK(17, 10) & (msg)->hdr) >> 10)

struct scmi_protocol {
	int id;
	struct k_event event;
};

struct scmi_message {
	struct rbnode node;
	uint32_t length;
	uint32_t hdr;
	void *data;
};

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_ */
