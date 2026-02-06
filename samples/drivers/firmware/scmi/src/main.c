/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(main);

int main(void)
{
	LOG_INF("Hello, world!");
}

/* filled in by each of the protocol modules */
SHELL_SUBCMD_SET_CREATE(scmi_cmds, (scmi));

SHELL_CMD_REGISTER(scmi, &scmi_cmds, "ARM SCMI commands", NULL);
