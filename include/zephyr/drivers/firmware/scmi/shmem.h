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

#define SCMI_SHMEM_CHAN_STATUS_BUSY_BIT BIT(0)
#define SCMI_SHMEM_CHAN_STATUS_ERR_BIT BIT(1)

#define SCMI_SHMEM_CHAN_FLAG_IRQ_BIT BIT(0)

int scmi_shmem_write_message(const struct device *shmem,
			     struct scmi_message *msg);

int scmi_shmem_read_message(const struct device *shmem,
			    struct scmi_message *msg);

void scmi_shmem_update_flags(const struct device *shmem,
			     uint32_t mask, uint32_t val);

#endif /* _INCLUDE_ZEPHYR_DRIVERS_FIRMWARE_SCMI_SHMEM_H_ */
