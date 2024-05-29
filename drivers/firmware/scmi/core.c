/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

static struct scmi_protocol *protocol_lut[] = {
};

static struct scmi_protocol *lookup_protocol(int id)
{
	int start, end, half;

	start = 0;
	end = ARRAY_SIZE(protocol_lut);
	half = end / 2;

	while (start != end) {
		if (protocol_lut[half].id == id) {
			return protocol_lut[half];
		}

		if (id > protocol_lut[half].id) {
			start = half;
		} else {
			end = half;
		}

		half = DIV_ROUND_UP((start + end), 2);
	}

	return NULL;
}

void scmi_core_work(struct k_work *work)
{
	int ret;
	struct scmi_message msg;
	struct scmi_channel *chan;
	struct scmi_protocol *proto;

	chan = CONTAINER_OF(work, struct scmi_channel, work);

	ret = scmi_transport_recv_message(chan, &msg);
	if (ret < 0) {
		return ret;
	}

	/* TODO: check if notification or reply (should be done
	 * based on channel type: RX or TX.
	 */
	proto = lookup_protocol(SCMI_PROTOCOL_ID(&msg.hdr));

	__ASSERT(proto, "got reply for unregistered protocol: %d", proto->id);

	k_event_set(&proto->event, msg.hdr);
}
