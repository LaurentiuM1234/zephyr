/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_

#include <zephyr/device.h>

struct scmi_message {
	uint32_t length;
	uint32_t hdr;
	void *data;
};

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_PROTOCOL_H_ */
