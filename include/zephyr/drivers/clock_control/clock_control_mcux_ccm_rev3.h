/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCUX_CCM_H_
#define ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCUX_CCM_H_

#include <zephyr/device.h>
#include <errno.h>

/** @brief Clock state.
 *
 * This structure is used to represent the states
 * a CCM clock may be found in.
 */
enum imx_ccm_clock_state {
	/** Clock is currently gated. */
	IMX_CCM_CLOCK_STATE_GATED = 0,
	/** Clock is currently ungated. */
	IMX_CCM_CLOCK_STATE_UNGATED,
};

/** @brief Clock structure.
 *
 * This is the most important structure, used to represent a clock
 * in the CCM. The CCM Rev3 driver only knows how to operate with this
 * structure.
 *
 * If you ever need to store more data about your clock then just
 * create your new clock structure, which will contain a struct
 * imx_ccm_clock as its first member. Using this strategy, you
 * can just cast your clock data to a generic struct imx_ccm_clock *
 * which can be safely used by CCM Rev3 driver. An example of this
 * can be seen in imx93_ccm.c.
 */
struct imx_ccm_clock {
	/** NXP HAL clock encoding. */
	uint32_t id;
	/** Clock frequency. */
	uint32_t freq;
	/** Clock state. */
	enum imx_ccm_clock_state state;
	/** True if clock allows gating operations */
	bool allows_gating;
};

/** @brief Clock operations.
 *
 * Since many clock operations are SoC-dependent, this structure
 * provides a set of operations each SoC needs to define to assure
 * the functionality of CCM Rev3.
 */
struct imx_ccm_clock_api {
	int (*on_off)(const struct device *dev, uint32_t clk_id, bool on);
	int (*set_clock_rate)(const struct device *dev, uint32_t clk_id, uint32_t rate);
	int (*get_clock)(uint32_t clk_id, struct imx_ccm_clock **clk);
	int (*assign_parent)(const struct device *dev, uint32_t clk_id, uint32_t parent_id);
	bool (*rate_is_valid)(const struct device *dev, uint32_t clk_id, uint32_t rate);
};

struct imx_ccm_config {
	uint32_t regmap_phys;
	uint32_t pll_regmap_phys;

	uint32_t regmap_size;
	uint32_t pll_regmap_size;

	uint32_t *clocks;
	uint32_t clock_num;

	uint32_t *parents;
	uint32_t parent_num;

	uint32_t *rates;
	uint32_t rate_num;

	uint32_t *flags;
	uint32_t flag_num;
};

struct imx_ccm_data {
	mm_reg_t regmap;
	mm_reg_t pll_regmap;
	struct imx_ccm_clock_api *api;
};

/**
 * @brief Validate if rate is valid for a clock.
 *
 * This function checks if a given rate is valid for a given clock.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param clk_id Clock ID.
 * @param rate Clock frequency.
 *
 * @retval true if rate is valid for clock, false otherwise.
 */
static inline bool imx_ccm_rate_is_valid(const struct device *dev,
					 uint32_t clk_id, uint32_t rate)
{
	struct imx_ccm_data *data = dev->data;

	if (!data->api || !data->api->rate_is_valid) {
		return -EINVAL;
	}

	return data->api->rate_is_valid(dev, clk_id, rate);
}

/**
 * @brief Assign a clock parent.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param clk_id Clock ID.
 * @param parent_id Parent ID.
 *
 * @retval 0 if successful, negative value otherwise.
 */
static inline int imx_ccm_assign_parent(const struct device *dev,
					uint32_t clk_id, uint32_t parent_id)
{
	struct imx_ccm_data *data = dev->data;

	if (!data->api || !data->api->on_off) {
		return -EINVAL;
	}

	return data->api->assign_parent(dev, clk_id, parent_id);
}

/**
 * @brief Turn on or off a clock.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param clk_id Clock ID.
 * @param on Should the clock be turned on or off?
 *
 * @retval 0 if successful, negative value otherwise.
 */
static inline int imx_ccm_on_off(const struct device *dev, uint32_t clk_id, bool on)
{
	struct imx_ccm_data *data = dev->data;

	if (!data->api || !data->api->on_off) {
		return -EINVAL;
	}

	return data->api->on_off(dev, clk_id, on);
}

/**
 * @brief Set a clock's frequency.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param clk_id Clock ID.
 * @param rate The requested frequency.
 *
 * @retval positive value representing the obtained clock rate.
 * @retval -ENOTSUP if rate is not supported.
 * @retval negative value if any error occurs.
 */
static inline int imx_ccm_set_clock_rate(const struct device *dev,
					 uint32_t clk_id,
					 uint32_t rate)
{
	struct imx_ccm_data *data = dev->data;

	if (!data->api || !data->api->set_clock_rate) {
		return -EINVAL;
	}

	return data->api->set_clock_rate(dev, clk_id, rate);
}

/**
 * @brief Retrieve a clock's data.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param clk_id Clock ID.
 * @param clk Clock data
 *
 * @retval 0 if successful, negative value otherwise.
 */
static inline int imx_ccm_get_clock(const struct device *dev,
				    uint32_t clk_id,
				    struct imx_ccm_clock **clk)
{
	struct imx_ccm_data *data = dev->data;

	if (!data->api || !data->api->get_clock) {
		return -EINVAL;
	}

	return data->api->get_clock(clk_id, clk);
}

/**
 * @brief Perform SoC-specific CCM initialization.
 *
 * Apart from SoC-specific initialization, it's expected that this
 * function will also set the CCM driver API from struct imx_ccm_data.
 *
 * @param dev Pointer to the device structure for the driver instance.
 *
 * @retval 0 if successful, negative value otherwise.
 */
int imx_ccm_init(const struct device *dev);

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_MCUX_CCM_H_ */
