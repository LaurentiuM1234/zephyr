/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_

#include <zephyr/device.h>

/* TODO: add node - this needs to be in decimal */
#define SCMI_PROTOCOL_BASE 16

struct scmi_channel;

struct scmi_protocol {
	struct scmi_channel *tx;
	struct scmi_channel *rx;
	const struct device *core;
};

struct scmi_message {
	uint32_t hdr;
};

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_ */
