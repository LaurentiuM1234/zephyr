/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_SHMEM_H_
#define _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_SHMEM_H_

#include <zephyr/device.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/drivers/firmware/scmi/protocol.h>
#include <errno.h>

#define SCMI_SHMEM_CHANNEL_BUSY_MASK BIT(0)
#define SCMI_SHMEM_CHANNEL_ERROR_MASK BIT(1)

#define SCMI_SHMEM_CHANNEL_FLAG_INT_EN_MASK BIT(0)

int scmi_shmem_is_busy(const struct device *dev);
int scmi_shmem_write_message(const struct device *dev, struct scmi_message *msg);
int scmi_shmem_read_message(const struct device *dev, struct scmi_message *msg);
void scmi_shmem_update_channel_status(const struct device *dev, uint32_t set,
				      uint32_t clear);
void scmi_shmem_update_channel_flags(const struct device *dev, uint32_t set,
				     uint32_t clear);

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_SHMEM_H_ */
