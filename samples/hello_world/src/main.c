/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/dt-bindings/clock/imx93_ccm.h>
#include <zephyr/drivers/clock_control/clock_control_mcux_ccm_rev3.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(main);

static inline int validate_clock_data(const struct device *dev,
				      uint32_t clk_id,
				      uint32_t expected_state,
				      uint32_t expected_rate)
{
	int ret;
	struct imx_ccm_clock *clk;
	uint32_t rate;

	ret = clock_control_get_rate(dev, (void *)(uintptr_t)clk_id, &rate);
	if (ret < 0) {
		LOG_ERR("Failed to get clock 0x%x rate: %d\n", clk_id, ret);
		return ret;
	}

	ret = imx_ccm_get_clock(dev, clk_id, &clk);
	if (ret < 0) {
		LOG_ERR("Failed to get clock 0x%x handle: %d\n", clk_id, ret);
		return ret;
	}

	if (rate != expected_rate) {
		LOG_ERR("clock 0x%x's rate should be %d. Got: %d\n", clk_id,
		       expected_rate, rate);
		return rate;
	}

	if (clk->state != expected_state) {
		LOG_ERR("clock 0x%x's state should be %d. Got: %d\n", clk_id,
		       expected_state, clk->state);
		return -EINVAL;
	}

	return 0;
}

static inline int validate_clock_set_data(const struct device *dev,
					  uint32_t clk_id,
					  uint32_t new_rate,
					  uint32_t expected_rate,
					  uint32_t old_rate)
{
	int ret;
	struct imx_ccm_clock *clk;

	ret = imx_ccm_get_clock(dev, clk_id, &clk);
	if (ret < 0) {
		LOG_ERR("Failed to get clock 0x%x handle: %d\n", clk_id, ret);
		return ret;
	}

	/* clock is in INIT. This is expected to fail. */
	ret = clock_control_set_rate(dev, (void *)(uintptr_t)clk_id,
				     (void *)(uintptr_t)new_rate);
	if (ret > 0) {
		LOG_ERR("set_rate() should fail for INIT clocks: 0x%x\n", clk_id);
		return ret;
	}

	/* this shouldn't fail. Transition to GATED state */
	ret = clock_control_off(dev, (void *)(uintptr_t)clk_id);
	if (ret < 0) {
		LOG_ERR("Failed to gate clock 0x%x\n", clk_id);
		return ret;
	}

	if (clk->state != IMX_CCM_CLOCK_STATE_GATED) {
		LOG_ERR("Clock's state should be GATED: 0x%x", clk_id);
		return -EINVAL;
	}

	/* this should fail with -EAGAIN */
	ret = clock_control_set_rate(dev, (void *)(uintptr_t)clk_id,
				     (void *)(uintptr_t)old_rate);
	if (ret != -EALREADY) {
		LOG_ERR("set_rate() should fail with -EALREADY: 0x%x\n", clk_id);
		return ret;
	}

	/* this shouldn't fail */
	ret = clock_control_set_rate(dev, (void *)(uintptr_t)clk_id,
				     (void *)(uintptr_t)new_rate);
	if (ret != expected_rate) {
		LOG_ERR("expected rate: %d, got: %d for 0x%x\n", expected_rate, ret, clk_id);
		return ret;
	}


	if (clk->freq != expected_rate) {
		LOG_ERR("Clock data's freq is %d, expected %d for 0x%x\n",
		       clk->freq, expected_rate, clk_id);
		return -EINVAL;
	}

	/* this should fail with -ENOTSUP */
	ret = clock_control_set_rate(dev, (void *)(uintptr_t)clk_id,
				     (void *)(uintptr_t)1000000000);
	if (ret != -ENOTSUP) {
		LOG_ERR("set_rate() should have failed with -ENOTSUP: 0x%x", clk_id);
		return ret;
	}

	/* this shouldn't fail. Transition to UNGATED state */
	ret = clock_control_on(dev, (void *)(uintptr_t)clk_id);
	if (ret < 0) {
		LOG_ERR("Failed to ungate clock 0x%x\n", clk_id);
		return ret;
	}

	return 0;
}

int main(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ccm_dummy));
	int ret;

	/* validate OSC_24M data */
	ret = validate_clock_data(dev, IMX93_CCM_OSC_24M,
				  IMX_CCM_CLOCK_STATE_INIT,
				  24000000);
	if (ret < 0) {
		return ret;
	}

	/* validate SYS_PLL1_PFD0 data */
	ret = validate_clock_data(dev, IMX93_CCM_SYS_PLL1_PFD0_DIV2,
				  IMX_CCM_CLOCK_STATE_INIT,
				  500000000);
	if (ret < 0) {
		return ret;
	}

	/* validate SYS_PLL1_PFD1 data */
	ret = validate_clock_data(dev, IMX93_CCM_SYS_PLL1_PFD1_DIV2,
				  IMX_CCM_CLOCK_STATE_INIT,
				  400000000);
	if (ret < 0) {
		return ret;
	}

	/* validate LPUART1_ROOT data */
	ret = validate_clock_data(dev, IMX93_CCM_LPUART1_ROOT,
				  IMX_CCM_CLOCK_STATE_INIT,
				  24000000);
	if (ret < 0) {
		return ret;
	}

	/* validate LPUART1_ROOT data */
	ret = validate_clock_data(dev, IMX93_CCM_LPUART2_ROOT,
				  IMX_CCM_CLOCK_STATE_INIT,
				  24000000);
	if (ret < 0) {
		return ret;
	}

	/* validate LPUART1 data */
	ret = validate_clock_data(dev, IMX93_CCM_LPUART1,
				  IMX_CCM_CLOCK_STATE_INIT,
				  24000000);
	if (ret < 0) {
		return ret;
	}

	/* validate LPUART2 data */
	ret = validate_clock_data(dev, IMX93_CCM_LPUART2,
				  IMX_CCM_CLOCK_STATE_INIT,
				  24000000);
	if (ret < 0) {
		return ret;
	}

	/* validate request rate operation */
	ret = validate_clock_set_data(dev, IMX93_CCM_LPUART8,
				      12000000,
				      12000000,
				      24000000);
	if (ret < 0) {
		return ret;
	}

	while(true) {
		k_busy_wait(10000);
		printf("hello, world!\n");
	}

	LOG_INF("Test done.\n");

	return 0;
}
