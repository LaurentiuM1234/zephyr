/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/firmware/scmi/pinctrl.h>

DT_SCMI_PROTOCOL_DEFINE(DT_INST(0, arm_scmi_pinctrl), NULL);

int scmi_pinctrl_settings_configure(struct scmi_pinctrl_settings *settings)
{
	struct scmi_protocol *proto;
	struct scmi_message msg, reply;
	uint32_t config_num;
	int32_t status, ret;

	proto = &DT_SCMI_PROTOCOL_NAME(DT_INST(0, arm_scmi_pinctrl));

	/* sanity checks */
	if (!settings) {
		return -EINVAL;
	}

	config_num = SCMI_PINCTRL_ATTRIBUTES_CONFIG_NUM(settings->attributes);

	if (!config_num) {
		return -EINVAL;
	}

	if ((config_num * 2) > ARM_SCMI_PINCTRL_MAX_CONFIG_SIZE) {
		return -EINVAL;
	}

	msg.hdr = SCMI_MESSAGE_HDR_MAKE(SCMI_PINCTRL_MSG_PINCTRL_SETTINGS_CONFIGURE,
					SCMI_COMMAND, proto->id, 0x0);
	msg.len = sizeof(*settings) -
		(ARM_SCMI_PINCTRL_MAX_CONFIG_SIZE - config_num * 2) * 4;
	msg.content = settings;

	reply.hdr = msg.hdr;
	reply.len = sizeof(status);
	reply.content = &status;

	ret = scmi_core_send_message(proto, &msg, &reply);
	if (ret < 0) {
		return ret;
	}

	if (status < 0) {
		return scmi_ret_to_linux(status);
	}

	return 0;
}
