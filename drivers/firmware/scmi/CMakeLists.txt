# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources(shmem.c)
zephyr_library_sources(mailbox.c)
zephyr_library_sources(core.c)
zephyr_library_sources_ifdef(CONFIG_ARM_SCMI_CLK_HELPERS clk.c)
zephyr_library_sources(pinctrl.c)
