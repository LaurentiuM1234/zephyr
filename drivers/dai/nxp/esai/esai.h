/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_DAI_NXP_ESAI_H_
#define ZEPHYR_DRIVERS_DAI_NXP_ESAI_H_

#include <zephyr/drivers/dai.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>

#include <fsl_esai.h>

LOG_MODULE_REGISTER(nxp_dai_esai);

/* used for binding the driver */
#define DT_DRV_COMPAT nxp_dai_esai

/* workaround the fact that device_map() doesn't exist for SoCs with no MMU */
#ifndef DEVICE_MMIO_IS_IN_RAM
#define device_map(virt, phys, size, flags) *(virt) = (phys)
#endif /* DEVICE_MMIO_IS_IN_RAM */

/* utility macros */
#define UINT_TO_ESAI(x) ((ESAI_Type *)(uintptr_t)(x))

/* use to query the state of RX/TX. Make sure the direction is either
 * DAI_DIR_TX or DAI_DIR_RX before using this.
 */
#define ESAI_TX_RX_STATE(dir, data)\
	((dir) == DAI_DIR_TX ? (data)->tx_state : (data)->rx_state)

struct esai_config {
	uint32_t regmap_phys;
	uint32_t regmap_size;
};

struct esai_data {
	mm_reg_t regmap;
	struct dai_config cfg;
	enum dai_state tx_state;
	enum dai_state rx_state;
};

/* this needs to perfectly match SOF's struct sof_ipc_dai_esai_params */
struct esai_bespoke_config {
	uint32_t reserved0;

	uint16_t reserved1;
	uint16_t mclk_id;
	uint32_t mclk_direction;

	/* CLOCK-related data */
	uint32_t mclk_rate;
	uint32_t fsync_rate;
	uint32_t bclk_rate;

	/* TDM-related data */
	uint32_t tdm_slots;
	uint32_t rx_slots;
	uint32_t tx_slots;
	uint16_t tdm_slot_width;

	uint16_t reserved2;
};

/* TODO: update as more state transitions become possible */
static int esai_update_state(enum dai_dir dir,
			     struct esai_data *data, enum dai_state new_state
{
	enum dai_state old_state = ESAI_TX_RX_STATE(dir, data);

	if (dir != DAI_DIR_TX && dir != DAI_DIR_RX) {
		LOG_ERR("invalid direction: %d", dir);
		return -EINVAL;
	}

	LOG_DBG("attempting transition from %d to %d", old_state, new_state);

	/* check if transition is possible */
	switch (new_state) {
	case DAI_STATE_NOT_READY:
		/* this transition is not possible with current design */
		return -EPERM;
	case DAI_STATE_READY:
		if (old_state != DAI_STATE_NOT_READY) {
			return -EPERM;
		}
	default:
		LOG_ERR("invalid new state: %d", new_state);
		return -EINVAL;
	}

	if (dir == DAI_DIR_RX) {
		data->rx_state = new_state;
	} else {
		data->tx_state = new_state;
	}
}

#endif /* ZEPHYR_DRIVERS_DAI_NXP_ESAI_H_ */
