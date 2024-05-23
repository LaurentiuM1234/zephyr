/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr/drivers/firmware/scmi/protocol.h>
#include <zephyr/drivers/firmware/scmi/transport.h>

struct cmd_data {
	int status;
	uint32_t version;
};

int main(void)
{
	const struct device *scmi_dev = DEVICE_DT_GET(DT_NODELABEL(scmi_dev));
	const struct scmi_transport_api *api = scmi_dev->api;
	struct scmi_message msg, reply;
	struct cmd_data cmd_data;
	int ret;

	cmd_data.status = 0xcafebabe;
	cmd_data.version = 0xdeadbeef;

	if (!api->send_message) {
		return -1;
	}

	msg.length = 0;
	msg.hdr =  (((uint32_t)0x10) << 10);

	reply.length = 0x8;
	reply.data = &cmd_data;

	ret = api->send_message(scmi_dev, &msg, &reply);
	if (ret) {
		return -1;
	}

	uint32_t hdr = reply.hdr;
	int status = cmd_data.status;
	uint32_t version = cmd_data.version;

	printf("HDR: 0x%x, STATUS: 0x%x, VERSION: 0x%x\n", hdr, status,
	       version);

	while (1);

	return 0;
}
