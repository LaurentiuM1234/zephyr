# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)
add_subdirectory(drivers)

zephyr_sources_ifdef(CONFIG_ARM_MMU mmu_regions.c)
