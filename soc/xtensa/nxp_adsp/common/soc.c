/*
 * Copyright (c) 2021 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/arch/xtensa/irq.h>
#include <zephyr/device.h>
#include <xtensa/xtruntime.h>
#include <zephyr/irq_nextlevel.h>
#include <xtensa/hal.h>
#include <zephyr/init.h>

#include "soc.h"

#ifdef CONFIG_DYNAMIC_INTERRUPTS
#include <zephyr/sw_isr_table.h>
#endif
#define LOG_LEVEL CONFIG_SOC_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(soc);


/* temporary workaround the fact that not all i.MX8 SoCs use IRQ_STEER driver */
#ifndef CONFIG_NXP_IRQSTEER

void z_soc_irq_enable(uint32_t irq)
{
	/*
	 * enable core interrupt
	 */
	z_xtensa_irq_enable(XTENSA_IRQ_NUMBER(irq));
}

void z_soc_irq_disable(uint32_t irq)
{
	/*
	 * disable the interrupt in interrupt controller
	 */
	z_xtensa_irq_disable(XTENSA_IRQ_NUMBER(irq));
}

int z_soc_irq_is_enabled(unsigned int irq)
{
	int ret = 0;

	/* regular interrupt */
	ret = z_xtensa_irq_is_enabled(XTENSA_IRQ_NUMBER(irq));

	return ret;
}

#endif /* CONFIG_NXP_IRQSTEER */
