// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2009-2010, Lars-Peter Clausen <lars@metafoo.de>
 *  JZ4740 platform devices
 *  Copyright (C) 2017 Paul Boddie <paul@boddie.org.uk>
 *  JZ4730 customisations
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/resource.h>

#include <linux/dma-mapping.h>

#include <linux/usb/musb.h>

#include <asm/mach-jz4740/platform.h>
#include <asm/mach-jz4740/base.h>
#include <asm/mach-jz4740/irq.h>

#include <linux/serial_core.h>
#include <linux/serial_8250.h>

/* USB Device Controller */
struct platform_device jz4740_udc_xceiv_device = {
	.name = "usb_phy_generic",
	.id   = 0,
};

static struct resource jz4740_udc_resources[] = {
	[0] = {
		.start = JZ4740_UDC_BASE_ADDR,
		.end   = JZ4740_UDC_BASE_ADDR + 0x10000 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
#ifdef CONFIG_MACH_JZ4730
		.start = JZ4730_IRQ_UDC,
		.end   = JZ4730_IRQ_UDC,
#else
		.start = JZ4740_IRQ_UDC,
		.end   = JZ4740_IRQ_UDC,
#endif
		.flags = IORESOURCE_IRQ,
		.name  = "mc",
	},
};

struct platform_device jz4740_udc_device = {
	.name = "musb-jz4740",
	.id   = -1,
	.dev  = {
		.dma_mask          = &jz4740_udc_device.dev.coherent_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.num_resources = ARRAY_SIZE(jz4740_udc_resources),
	.resource      = jz4740_udc_resources,
};

/* NAND controller */
#ifdef CONFIG_MACH_JZ4730
static struct resource jz4740_nand_resources[] = {
	{
		.name	= "mmio",
		.start	= JZ4740_EMC_BASE_ADDR,
		.end	= JZ4740_EMC_BASE_ADDR + 0x1000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "bank1",
		.start	= 0x18000000,
		.end	= 0x180C0000 - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name	= "bank2",
		.start	= 0x14000000,
		.end	= 0x140C0000 - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name	= "bank3",
		.start	= 0x0C000000,
		.end	= 0x0C0C0000 - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.name	= "bank4",
		.start	= 0x08000000,
		.end	= 0x080C0000 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device jz4740_nand_device = {
	.name = "jz4740-nand",
	.num_resources = ARRAY_SIZE(jz4740_nand_resources),
	.resource = jz4740_nand_resources,
};
#endif

/* LCD controller */
static struct resource jz4740_framebuffer_resources[] = {
	{
		.start	= JZ4740_LCD_BASE_ADDR,
		.end	= JZ4740_LCD_BASE_ADDR + 0x1000 - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device jz4740_framebuffer_device = {
	.name		= "jz4740-fb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(jz4740_framebuffer_resources),
	.resource	= jz4740_framebuffer_resources,
	.dev = {
		.dma_mask = &jz4740_framebuffer_device.dev.coherent_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

/* I2S controller */
static struct resource jz4740_i2s_resources[] = {
	{
		.start	= JZ4740_AIC_BASE_ADDR,
		.end	= JZ4740_AIC_BASE_ADDR + 0x38 - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device jz4740_i2s_device = {
	.name		= "jz4740-i2s",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(jz4740_i2s_resources),
	.resource	= jz4740_i2s_resources,
};

/* PCM */
struct platform_device jz4740_pcm_device = {
	.name		= "jz4740-pcm-audio",
	.id		= -1,
};

/* Codec */
static struct resource jz4740_codec_resources[] = {
	{
		.start	= JZ4740_AIC_BASE_ADDR + 0x80,
		.end	= JZ4740_AIC_BASE_ADDR + 0x88 - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device jz4740_codec_device = {
	.name		= "jz4740-codec",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(jz4740_codec_resources),
	.resource	= jz4740_codec_resources,
};

/* ADC controller */
#ifndef CONFIG_MACH_JZ4730
static struct resource jz4740_adc_resources[] = {
	{
		.start	= JZ4740_SADC_BASE_ADDR,
		.end	= JZ4740_SADC_BASE_ADDR + 0x30,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= JZ4740_IRQ_SADC,
		.end	= JZ4740_IRQ_SADC,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= JZ4740_IRQ_ADC_BASE,
		.end	= JZ4740_IRQ_ADC_BASE,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device jz4740_adc_device = {
	.name		= "jz4740-adc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(jz4740_adc_resources),
	.resource	= jz4740_adc_resources,
};
#endif

/* PWM */
#ifdef CONFIG_MACH_JZ4730
struct platform_device jz4730_pwm_device = {
	.name = "jz4730-pwm",
#else
struct platform_device jz4740_pwm_device = {
	.name = "jz4740-pwm",
#endif
	.id   = -1,
};

/* DMA */
static struct resource jz4740_dma_resources[] = {
	{
		.start	= JZ4740_DMAC_BASE_ADDR,
		.end	= JZ4740_DMAC_BASE_ADDR + 0x400 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
#ifdef CONFIG_MACH_JZ4730
		.start	= JZ4730_IRQ_DMAC,
		.end	= JZ4730_IRQ_DMAC,
#else
		.start	= JZ4740_IRQ_DMAC,
		.end	= JZ4740_IRQ_DMAC,
#endif
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device jz4740_dma_device = {
	.name		= "jz4740-dma",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(jz4740_dma_resources),
	.resource	= jz4740_dma_resources,
};
