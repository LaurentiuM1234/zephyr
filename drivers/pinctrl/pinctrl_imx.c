/*
 * Copyright (c) 2022 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/arch/cpu.h>
#include <zephyr/init.h>
#include <zephyr/drivers/pinctrl.h>

#ifdef CONFIG_SOC_MIMX9596
#include <zephyr/drivers/firmware/scmi/pinctrl.h>
#endif /* CONFIG_SOC_MIMX9596 */

#ifdef CONFIG_SOC_MIMX9596
static int pinctrl_soc_configure_pin(const pinctrl_soc_pin_t *pin)
{
	struct scmi_pinctrl_settings settings[4];
	int ret, settings_num, i;
	uint32_t attributes;

	settings_num = 0;

	attributes = SCMI_PINCTRL_CONFIG_ATTRIBUTES(0x0, 0x1, SCMI_PINCTRL_SELECTOR_PIN);

	/* set mux value */
	settings[0].attributes = attributes;
	settings[0].id = pin->pinmux.mux_register / 4;
	settings[0].config[0] = IMX95_TYPE_MUX;
	settings[0].config[1] = IMX95_CONFIG_HAS_SION(pin->pin_ctrl_flags) ?
		(pin->pinmux.mux_mode | IMX95_PAD_SION_BIT) :
		pin->pinmux.mux_mode;
	settings_num++;

	/* set config value */
	settings[1].attributes = attributes;
	settings[1].id = (pin->pinmux.config_register - IMX95_CFG_BASE) / 4;
	settings[1].config[0] = IMX95_TYPE_CONFIG;
	settings[1].config[1] = pin->pin_ctrl_flags & ~IMX95_CONFIG_SION_BIT;
	settings_num++;

	/* set daisy - optional */
	if (pin->pinmux.input_register) {
		settings[2].attributes = attributes;
		settings[2].id = (pin->pinmux.input_register - IMX95_DAISY_BASE) / 4;
		settings[2].config[0] = IMX95_TYPE_DAISY_ID;
		settings[2].config[1] = 0x0; /* not relevant */
		settings_num++;

		settings[3].attributes = attributes;
		settings[3].id = 0x0; /* not relevant */
		settings[3].config[0] = IMX95_TYPE_DAISY_CFG;
		settings[3].config[1] = pin->pinmux.input_daisy;
		settings_num++;
	}

	for (i = 0; i < settings_num; i++) {
		ret = scmi_pinctrl_settings_configure(&settings[i]);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}
#endif /* CONFIG_SOC_MIMX9596 */

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt,
			   uintptr_t reg)
{
	/* configure all pins */
	for (uint8_t i = 0U; i < pin_cnt; i++) {
		uint32_t mux_register = pins[i].pinmux.mux_register;
		uint32_t mux_mode = pins[i].pinmux.mux_mode;
		uint32_t input_register = pins[i].pinmux.input_register;
		uint32_t input_daisy = pins[i].pinmux.input_daisy;
		uint32_t config_register = pins[i].pinmux.config_register;
		uint32_t pin_ctrl_flags = pins[i].pin_ctrl_flags;
#if defined(CONFIG_SOC_SERIES_IMXRT10XX) || defined(CONFIG_SOC_SERIES_IMXRT11XX)
		volatile uint32_t *gpr_register =
			(volatile uint32_t *)((uintptr_t)pins[i].pinmux.gpr_register);
		if (gpr_register) {
			/* Set or clear specified GPR bit for this mux */
			if (pins[i].pinmux.gpr_val) {
				*gpr_register |=
					(pins[i].pinmux.gpr_val << pins[i].pinmux.gpr_shift);
			} else {
				*gpr_register &= ~(0x1 << pins[i].pinmux.gpr_shift);
			}
		}
#endif

#ifdef CONFIG_SOC_MIMX9352_A55
		sys_write32(IOMUXC1_SW_MUX_CTL_PAD_MUX_MODE(mux_mode) |
			IOMUXC1_SW_MUX_CTL_PAD_SION(MCUX_IMX_INPUT_ENABLE(pin_ctrl_flags)),
			(mem_addr_t)mux_register);
		if (input_register) {
			sys_write32(IOMUXC1_SELECT_INPUT_DAISY(input_daisy),
				    (mem_addr_t)input_register);
		}
		if (config_register) {
			sys_write32(pin_ctrl_flags & (~(0x1 << MCUX_IMX_INPUT_ENABLE_SHIFT)),
				    (mem_addr_t)config_register);
		}
#elif defined(CONFIG_SOC_MIMX8UD7)
		if (mux_register == config_register) {
			sys_write32(IOMUXC_PCR_MUX_MODE(mux_mode) |
				    pin_ctrl_flags, (mem_addr_t)mux_register);
		} else {
			sys_write32(IOMUXC_PCR_MUX_MODE(mux_mode),
				    (mem_addr_t)mux_register);

			if (config_register) {
				sys_write32(pin_ctrl_flags, (mem_addr_t)config_register);
			}
		}

		if (input_register) {
			sys_write32(IOMUXC_PSMI_SSS(input_daisy), (mem_addr_t)input_register);
		}
#elif defined(CONFIG_SOC_MIMX9596)
		int ret = pinctrl_soc_configure_pin(&pins[i]);
		if (ret < 0) {
			return ret;
		}
#else
		sys_write32(
			IOMUXC_SW_MUX_CTL_PAD_MUX_MODE(mux_mode) |
				IOMUXC_SW_MUX_CTL_PAD_SION(MCUX_IMX_INPUT_ENABLE(pin_ctrl_flags)),
			(mem_addr_t)mux_register);
		if (input_register) {
			sys_write32(IOMUXC_SELECT_INPUT_DAISY(input_daisy),
				    (mem_addr_t)input_register);
		}
		if (config_register) {
			sys_write32(pin_ctrl_flags & (~(0x1 << MCUX_IMX_INPUT_ENABLE_SHIFT)),
				    config_register);
		}
#endif
	}
	return 0;
}

static int imx_pinctrl_init(void)
{
#if defined(CONFIG_SOC_SERIES_IMXRT10XX) || defined(CONFIG_SOC_SERIES_IMXRT11XX)
	CLOCK_EnableClock(kCLOCK_Iomuxc);
#ifdef CONFIG_SOC_SERIES_IMXRT10XX
	CLOCK_EnableClock(kCLOCK_IomuxcSnvs);
	CLOCK_EnableClock(kCLOCK_IomuxcGpr);
#elif defined(CONFIG_SOC_SERIES_IMXRT11XX)
	CLOCK_EnableClock(kCLOCK_Iomuxc_Lpsr);
#endif /* CONFIG_SOC_SERIES_IMXRT10XX */
#elif defined(CONFIG_SOC_MIMX8MQ6)
	CLOCK_EnableClock(kCLOCK_Iomux);
#endif /* CONFIG_SOC_SERIES_IMXRT10XX || CONFIG_SOC_SERIES_IMXRT11XX */

	return 0;
}

SYS_INIT(imx_pinctrl_init, PRE_KERNEL_1, 0);
