/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SOC_NXP_IMX_IMX95_PINCTRL_SOC_H_
#define _SOC_NXP_IMX_IMX95_PINCTRL_SOC_H_

#define IMX95_CONFIG_SION_BIT BIT(30)
#define IMX95_CONFIG_HAS_SION(cfg) ((cfg) & IMX95_CONFIG_SION_BIT)

#define IMX95_PAD_SION_BIT BIT(4)

#define IMX95_TYPE_MUX 192
#define IMX95_TYPE_CONFIG 193
#define IMX95_TYPE_DAISY_ID 194
#define IMX95_TYPE_DAISY_CFG 195

#define IMX95_CFG_BASE 0x204
#define IMX95_DAISY_BASE 0x408

struct imx95_pinmux {
	uint32_t mux_register;
	uint32_t mux_mode;
	uint32_t input_register;
	uint32_t input_daisy;
	uint32_t config_register;
	uint32_t config;
};

struct imx95_pinctrl {
	struct imx95_pinmux pinmux;
	uint32_t pin_ctrl_flags;
};

typedef struct imx95_pinctrl pinctrl_soc_pin_t;

#define IMX95_PINMUX(node_id)					\
{								\
	.mux_register = DT_PROP_BY_IDX(node_id, pinmux, 0),	\
	.config_register = DT_PROP_BY_IDX(node_id, pinmux, 1),	\
	.input_register = DT_PROP_BY_IDX(node_id, pinmux, 2),	\
	.mux_mode = DT_PROP_BY_IDX(node_id, pinmux, 3),	\
	.input_daisy = DT_PROP_BY_IDX(node_id, pinmux, 4),	\
}

#define IMX95_PINCFG(node_id) DT_PROP_BY_IDX(node_id, pinmux, 5)

#define Z_PINCTRL_STATE_PIN_INIT(group_id, pin_prop, idx)				\
{											\
	.pinmux = IMX95_PINMUX(DT_PHANDLE_BY_IDX(group_id, pin_prop, idx)),		\
	.pin_ctrl_flags = IMX95_PINCFG(DT_PHANDLE_BY_IDX(group_id, pin_prop, idx)),	\
},

#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop)			\
	{ DT_FOREACH_CHILD_VARGS(DT_PHANDLE(node_id, prop),		\
				 DT_FOREACH_PROP_ELEM,			\
				 pinmux, Z_PINCTRL_STATE_PIN_INIT) }

#endif /* _SOC_NXP_IMX_IMX95_PINCTRL_SOC_H_ */
