/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <zephyr/drivers/firmware/scmi/clk.h>
#include <zephyr/drivers/firmware/scmi/protocol.h>
#include <zephyr/shell/shell.h>

extern struct scmi_protocol SCMI_PROTOCOL_NAME(SCMI_PROTOCOL_CLOCK);

struct clk_info {
	bool enabled;
	uint32_t rate;
	uint32_t parent_id;
	char name[SCMI_CLOCK_NAME_LEN];
	char parent_name[SCMI_CLOCK_NAME_LEN];
};

static int cmd_clk_get_version(const struct shell *sh, size_t argc, char **argv)
{
	struct scmi_protocol *proto;
	uint32_t version;
	int ret;

	proto = &SCMI_PROTOCOL_NAME(SCMI_PROTOCOL_CLOCK);

	ret = scmi_protocol_get_version(proto, &version);
	if (ret) {
		shell_error(sh, "failed to query protocol version: %d", ret);
		return ret;
	}

	shell_print(sh, "Clock protocol version: 0x%x", version);

	return 0;
}

static int clk_get_info(const struct shell *sh,
			struct scmi_protocol *proto,
			uint32_t clk_id,
			struct clk_info *info)
{
	struct scmi_clock_attributes attributes;
	int ret;

	memset(info, 0, sizeof(*info));

	ret = scmi_clock_attributes(proto, clk_id, &attributes);
	if (ret) {
		return 0;
	}

	memcpy(info->name, attributes.clock_name, SCMI_CLOCK_NAME_LEN);
	info->enabled = SCMI_CLOCK_ENABLED(attributes.attributes);

	ret = scmi_clock_rate_get(proto, clk_id, &info->rate);
	if (ret) {
		shell_error(sh, "failed to query clk %d rate: %d", clk_id, ret);
		return ret;
	}

	ret = scmi_clock_parent_get(proto, clk_id, &info->parent_id);
	if (ret) {
		return 0;
	}

	ret = scmi_clock_attributes(proto, info->parent_id, &attributes);
	if (ret) {
		shell_error(sh, "failed to query parent %d attributes: %d",
			    info->parent_id, ret);
		return ret;
	}

	memcpy(info->parent_name, attributes.clock_name, SCMI_CLOCK_NAME_LEN);

	return 0;
}

static int cmd_clk_get_summary(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t clk_num, attributes;
	struct scmi_protocol *proto;
	struct clk_info info;
	int ret, i;

	proto = &SCMI_PROTOCOL_NAME(SCMI_PROTOCOL_CLOCK);

	ret = scmi_clock_protocol_attributes(proto, &attributes);
	if (ret) {
		shell_error(sh, "Failed to query protocol attributes: %d", ret);
		return ret;
	}

	clk_num = SCMI_CLK_ATTRIBUTES_CLK_NUM(attributes);

	shell_print(sh,
		    "+----------------------------------------------------------------------+");
	shell_print(sh,
		    "| ID |        Name        | Enabled |   Rate(Hz)  |        Parent      |");
	shell_print(sh,
		    "+----------------------------------------------------------------------+");

	for (i = 0; i < clk_num; i++) {
		ret = clk_get_info(sh, proto, i, &info);
		if (ret) {
			shell_error(sh, "Failed to get clk %d summary: %d", i, ret);
			return ret;
		}

		if (!*info.name) {
			continue;
		}

		shell_print(sh, "|%3d |  %16s  |    %c    |%12u |  %16s  |",
			    i, info.name,
			    info.enabled ? 'Y' : 'N',
			    info.rate, *info.parent_name ? info.parent_name : "N/A\0");

		shell_print(sh,
			    "+----------------------------------------------------------------------+");
	}

	return 0;
}

static int cmd_clk_get_info(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t attributes, clk_num, parent_num, parent_id;
	struct scmi_clock_parents parents;
	struct scmi_protocol *proto;
	struct clk_info info;
	int clk_id, ret, i;

	proto = &SCMI_PROTOCOL_NAME(SCMI_PROTOCOL_CLOCK);
	clk_id = atoi(argv[1]);

	if (clk_id < 0) {
		shell_error(sh, "Clock ID cannot be negative");
		return -EINVAL;
	}

	ret = scmi_clock_protocol_attributes(proto, &attributes);
	if (ret) {
		shell_error(sh, "Failed to query protocol attributes: %d", ret);
		return ret;
	}

	clk_num = SCMI_CLK_ATTRIBUTES_CLK_NUM(attributes);

	if (clk_id >= clk_num) {
		shell_error(sh, "Clock ID out of bounds. Supported clock number: %d", clk_num);
		return -EINVAL;
	}

	ret = clk_get_info(sh, proto, clk_id, &info);
	if (ret) {
		shell_error(sh, "Failed to query clk %d info: %d", clk_id, ret);
		return ret;
	}

	ret = scmi_clock_possible_parents_get(proto, clk_id, 0x0, &parents);
	if (ret) {
		shell_error(sh, "Failed to query clk %d parents: %d", clk_id, ret);
		return ret;
	}

	shell_print(sh, "Name: %s", info.name);
	shell_print(sh, "Enabled status: %c", info.enabled ? 'Y' : 'N');
	shell_print(sh, "Rate (Hz): %u", info.rate);
	shell_print(sh, "Available parents:");

	parent_num = SCMI_CLOCK_PARENT_NUM(parents.flags);
	parent_id = info.parent_id;

	for (i = 0; i < parent_num; i++) {
		ret = clk_get_info(sh, proto, parents.possible_parents[i], &info);
		if (ret) {
			shell_error(sh, "Failed to query clk %d parent %d info: %d",
				    clk_id, parents.possible_parents[i], ret);
			return ret;
		}

		if (parents.possible_parents[i] != parent_id) {
			shell_print(sh, "    - [%d] %s",
				    parents.possible_parents[i], info.name);
		} else {
			shell_print(sh, "    - [%d] %s (selected)",
				    parents.possible_parents[i], info.name);
		}
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(clk_cmds,
	SHELL_CMD(version, NULL, "get protocol version", cmd_clk_get_version),
	SHELL_CMD(summary, NULL, "get clock tree summary", cmd_clk_get_summary),
	SHELL_CMD_ARG(info, NULL,
		      "get detailed clock information. Arguments: <clock_id>",
		      cmd_clk_get_info, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_SUBCMD_ADD((scmi), clk, &clk_cmds, "Clock protocol commands", NULL, 0, 0);
