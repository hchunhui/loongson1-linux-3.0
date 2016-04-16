/*
 * Copyright (c) 2012 Tang, Haifeng <tanghaifeng-gz@loongson.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/serial_reg.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <video/ls1xfb.h>

#include <asm/bootinfo.h>
#include <loongson1.h>
#include <prom.h>

#define DEFAULT_MEMSIZE			64	/* If no memsize provided */
#define DEFAULT_BUSCLOCK		133000000
#define DEFAULT_CPUCLOCK		266000000

#ifdef CONFIG_STMMAC_ETH
extern unsigned char *hwaddr;
char *tmp;
#endif

unsigned long cpu_clock_freq;
unsigned long ls1x_bus_clock;
EXPORT_SYMBOL(cpu_clock_freq);
EXPORT_SYMBOL(ls1x_bus_clock);

int prom_argc;
char **prom_argv, **prom_envp;
unsigned long memsize, highmemsize;

const char *get_system_type(void)
{
	return "LS232 Evaluation board-V1.0";
}

char *prom_getenv(char *envname)
{
	char **env = prom_envp;
	int i;

	i = strlen(envname);

	while (*env) {
		if (strncmp(envname, *env, i) == 0 && *(*env+i) == '=')
			return *env + i + 1;
		env++;
	}

	return 0;
}

static inline unsigned long env_or_default(char *env, unsigned long dfl)
{
	char *str = prom_getenv(env);
	return str ? simple_strtol(str, 0, 0) : dfl;
}

void __init prom_init_cmdline(void)
{
	char *c = &(arcs_cmdline[0]);
	int i;

	for (i = 1; i < prom_argc; i++) {
		strcpy(c, prom_argv[i]);
		c += strlen(prom_argv[i]);
		if (i < prom_argc-1)
			*c++ = ' ';
	}
	*c = 0;
}

void __init prom_init(void)
{
	prom_argc = fw_arg0;
	prom_argv = (char **)fw_arg1;
	prom_envp = (char **)fw_arg2;

	mips_machtype = MACH_LS232;
	system_state = SYSTEM_BOOTING;

	prom_init_cmdline();

	ls1x_bus_clock = env_or_default("busclock", DEFAULT_BUSCLOCK);
	cpu_clock_freq = env_or_default("cpuclock", DEFAULT_CPUCLOCK);
	memsize = env_or_default("memsize", DEFAULT_MEMSIZE);
	highmemsize = env_or_default("highmemsize", 0x0);

#if defined(CONFIG_LS1C_MACH)
	__raw_writel(__raw_readl(LS1X_MUX_CTRL0) & (~USBHOST_SHUT), LS1X_MUX_CTRL0);
	__raw_writel(__raw_readl(LS1X_MUX_CTRL1) & (~USBHOST_RSTN), LS1X_MUX_CTRL1);
	mdelay(60);
	/* reset stop */
	__raw_writel(__raw_readl(LS1X_MUX_CTRL1) | USBHOST_RSTN, LS1X_MUX_CTRL1);
#else
	/* 需要复位一次USB控制器，且复位时间要足够长，否则启动时莫名其妙的死机 */
	#if defined(CONFIG_LS1A_MACH)
	#define MUX_CTRL0 LS1X_MUX_CTRL0
	#define MUX_CTRL1 LS1X_MUX_CTRL1
	#elif defined(CONFIG_LS1B_MACH)
	#define MUX_CTRL0 LS1X_MUX_CTRL1
	#define MUX_CTRL1 LS1X_MUX_CTRL1
	#endif
//	__raw_writel(__raw_readl(MUX_CTRL0) | USB_SHUT, MUX_CTRL0);
//	__raw_writel(__raw_readl(MUX_CTRL1) & (~USB_RESET), MUX_CTRL1);
//	mdelay(10);
//	#if defined(CONFIG_USB_EHCI_HCD_LS1X) || defined(CONFIG_USB_OHCI_HCD_LS1X)
	/* USB controller enable and reset */
	__raw_writel(__raw_readl(MUX_CTRL0) & (~USB_SHUT), MUX_CTRL0);
	__raw_writel(__raw_readl(MUX_CTRL1) & (~USB_RESET), MUX_CTRL1);
	mdelay(60);
	/* reset stop */
	__raw_writel(__raw_readl(MUX_CTRL1) | USB_RESET, MUX_CTRL1);
//	#endif
#endif

#ifdef CONFIG_STMMAC_ETH
	tmp = prom_getenv("ethaddr");
	if (tmp) {
		sscanf(tmp, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", 
			&hwaddr[0], &hwaddr[1], &hwaddr[2], &hwaddr[3], &hwaddr[4], &hwaddr[5]);
	}
#endif

	pr_info("memsize=%ldMB, highmemsize=%ldMB\n", memsize, highmemsize);
}

void __init prom_free_prom_memory(void)
{
	free_init_pages("prom memory", 0x280, 2 << 20);
}

void __init prom_putchar(char c)
{
	int timeout;

	timeout = 1024;

	while (((readb(PORT(UART_LSR)) & UART_LSR_THRE) == 0)
			&& (timeout-- > 0))
		;

	writeb(c, PORT(UART_TX));
}

