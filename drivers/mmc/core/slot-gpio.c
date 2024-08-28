// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generic GPIO card-detect helper
 *
 * Copyright (C) 2011, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/mmc/host.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "slot-gpio.h"

struct mmc_gpio {
	struct gpio_desc *ro_gpio;
	struct gpio_desc *cd_gpio;
	struct gpio_desc *rs_gpio; /* reset line */
	irqreturn_t (*cd_gpio_isr)(int irq, void *dev_id);
	char *ro_label;
	char *cd_label;
	char *rs_label;
	u32 cd_debounce_delay_ms;
};

static irqreturn_t mmc_gpio_cd_irqt(int irq, void *dev_id)
{
	/* Schedule a card detection after a debounce timeout */
	struct mmc_host *host = dev_id;
	struct mmc_gpio *ctx = host->slot.handler_priv;

	host->trigger_card_event = true;
	mmc_detect_change(host, msecs_to_jiffies(ctx->cd_debounce_delay_ms));

	return IRQ_HANDLED;
}

int mmc_gpio_alloc(struct mmc_host *host)
{
	struct mmc_gpio *ctx = devm_kzalloc(host->parent,
					    sizeof(*ctx), GFP_KERNEL);

	if (ctx) {
		ctx->cd_debounce_delay_ms = 200;
		ctx->cd_label = devm_kasprintf(host->parent, GFP_KERNEL,
				"%s cd", dev_name(host->parent));
		if (!ctx->cd_label)
			return -ENOMEM;
		ctx->ro_label = devm_kasprintf(host->parent, GFP_KERNEL,
				"%s ro", dev_name(host->parent));
		if (!ctx->ro_label)
			return -ENOMEM;
		ctx->rs_label = devm_kasprintf(host->parent, GFP_KERNEL,
				"%s rs", dev_name(host->parent));
		if (!ctx->rs_label)
			return -ENOMEM;
		host->slot.handler_priv = ctx;
		host->slot.cd_irq = -EINVAL;
	}

	return ctx ? 0 : -ENOMEM;
}

int mmc_gpio_get_ro(struct mmc_host *host)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;

	if (!ctx || !ctx->ro_gpio)
		return -ENOSYS;

	return gpiod_get_value_cansleep(ctx->ro_gpio);
}
EXPORT_SYMBOL(mmc_gpio_get_ro);

int mmc_gpio_get_cd(struct mmc_host *host)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;
	int cansleep;

	if (!ctx || !ctx->cd_gpio)
		return -ENOSYS;

	cansleep = gpiod_cansleep(ctx->cd_gpio);
	return cansleep ?
		gpiod_get_value_cansleep(ctx->cd_gpio) :
		gpiod_get_value(ctx->cd_gpio);
}
EXPORT_SYMBOL(mmc_gpio_get_cd);

void mmc_gpio_set_rs(struct mmc_host *host, int state)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;

	if (!ctx || !ctx->rs_gpio)
		return;

	gpiod_set_value_cansleep(ctx->rs_gpio, state);
}
EXPORT_SYMBOL(mmc_gpio_set_rs);


int mmc_gpio_request_rs(struct mmc_host *host, unsigned int gpio, int low)
{
	struct mmc_gpio *ctx;
	int ret;
	int flags;

	if (!gpio_is_valid(gpio))
		return -EINVAL;

	ret = mmc_gpio_alloc(host);
	if (ret < 0)
		return ret;

	ctx = host->slot.handler_priv;
	if (low)
		flags = GPIOF_DIR_OUT | GPIOF_INIT_HIGH | GPIOF_ACTIVE_LOW;
	else
		flags = GPIOF_DIR_OUT | GPIOF_INIT_LOW;

	ret = devm_gpio_request_one(&host->class_dev, gpio,
				    flags, ctx->rs_label);
	if (ret < 0)
		return ret;

	ctx->rs_gpio = gpio_to_desc(gpio);

	return 0;
}
EXPORT_SYMBOL(mmc_gpio_request_rs);

void mmc_gpiod_request_cd_irq(struct mmc_host *host)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;
	int irq = -EINVAL;
	int ret;

	if (host->slot.cd_irq >= 0 || !ctx || !ctx->cd_gpio)
		return;

	/*
	 * Do not use IRQ if the platform prefers to poll, e.g., because that
	 * IRQ number is already used by another unit and cannot be shared.
	 */
	if (!(host->caps & MMC_CAP_NEEDS_POLL))
		irq = gpiod_to_irq(ctx->cd_gpio);

	if (irq >= 0) {
		if (!ctx->cd_gpio_isr)
			ctx->cd_gpio_isr = mmc_gpio_cd_irqt;
		ret = devm_request_threaded_irq(host->parent, irq,
			NULL, ctx->cd_gpio_isr,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			ctx->cd_label, host);
		if (ret < 0)
			irq = ret;
	}

	host->slot.cd_irq = irq;

	if (irq < 0)
		host->caps |= MMC_CAP_NEEDS_POLL;
}
EXPORT_SYMBOL(mmc_gpiod_request_cd_irq);

int mmc_gpio_set_cd_wake(struct mmc_host *host, bool on)
{
	int ret = 0;

	if (!(host->caps & MMC_CAP_CD_WAKE) ||
	    host->slot.cd_irq < 0 ||
	    on == host->slot.cd_wake_enabled)
		return 0;

	if (on) {
		ret = enable_irq_wake(host->slot.cd_irq);
		host->slot.cd_wake_enabled = !ret;
	} else {
		disable_irq_wake(host->slot.cd_irq);
		host->slot.cd_wake_enabled = false;
	}

	return ret;
}
EXPORT_SYMBOL(mmc_gpio_set_cd_wake);

/* Register an alternate interrupt service routine for
 * the card-detect GPIO.
 */
void mmc_gpio_set_cd_isr(struct mmc_host *host,
			 irqreturn_t (*isr)(int irq, void *dev_id))
{
	struct mmc_gpio *ctx = host->slot.handler_priv;

	WARN_ON(ctx->cd_gpio_isr);
	ctx->cd_gpio_isr = isr;
}
EXPORT_SYMBOL(mmc_gpio_set_cd_isr);

/**
 * mmc_gpio_free_rs - free the reset gpio
 * @host: mmc host
 *
 * It's provided only for cases that client drivers need to manually free
 * up the write-protection gpio requested by mmc_gpio_request_rs().
 */
void mmc_gpio_free_rs(struct mmc_host *host)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;
	int gpio;

	if (!ctx || !ctx->rs_gpio)
		return;

	gpio = desc_to_gpio(ctx->rs_gpio);
	ctx->rs_gpio = NULL;

	devm_gpio_free(&host->class_dev, gpio);
}
EXPORT_SYMBOL(mmc_gpio_free_rs);

/**
 * mmc_gpiod_request_cd - request a gpio descriptor for card-detection
 * @host: mmc host
 * @con_id: function within the GPIO consumer
 * @idx: index of the GPIO to obtain in the consumer
 * @override_active_level: ignore %GPIO_ACTIVE_LOW flag
 * @debounce: debounce time in microseconds
 *
 * Note that this must be called prior to mmc_add_host()
 * otherwise the caller must also call mmc_gpiod_request_cd_irq().
 *
 * Returns zero on success, else an error.
 */
int mmc_gpiod_request_cd(struct mmc_host *host, const char *con_id,
			 unsigned int idx, bool override_active_level,
			 unsigned int debounce)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;
	struct gpio_desc *desc;
	int ret;

	desc = devm_gpiod_get_index(host->parent, con_id, idx, GPIOD_IN);
	if (IS_ERR(desc))
		return PTR_ERR(desc);

	if (debounce) {
		ret = gpiod_set_debounce(desc, debounce);
		if (ret < 0)
			ctx->cd_debounce_delay_ms = debounce / 1000;
	}

	/* override forces default (active-low) polarity ... */
	if (override_active_level && !gpiod_is_active_low(desc))
		gpiod_toggle_active_low(desc);

	/* ... or active-high */
	if (host->caps2 & MMC_CAP2_CD_ACTIVE_HIGH)
		gpiod_toggle_active_low(desc);

	ctx->cd_gpio = desc;

	return 0;
}
EXPORT_SYMBOL(mmc_gpiod_request_cd);

bool mmc_can_gpio_cd(struct mmc_host *host)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;

	return ctx->cd_gpio ? true : false;
}
EXPORT_SYMBOL(mmc_can_gpio_cd);

/**
 * mmc_gpiod_request_ro - request a gpio descriptor for write protection
 * @host: mmc host
 * @con_id: function within the GPIO consumer
 * @idx: index of the GPIO to obtain in the consumer
 * @debounce: debounce time in microseconds
 *
 * Returns zero on success, else an error.
 */
int mmc_gpiod_request_ro(struct mmc_host *host, const char *con_id,
			 unsigned int idx, unsigned int debounce)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;
	struct gpio_desc *desc;
	int ret;

	desc = devm_gpiod_get_index(host->parent, con_id, idx, GPIOD_IN);
	if (IS_ERR(desc))
		return PTR_ERR(desc);

	if (debounce) {
		ret = gpiod_set_debounce(desc, debounce);
		if (ret < 0)
			return ret;
	}

	if (host->caps2 & MMC_CAP2_RO_ACTIVE_HIGH)
		gpiod_toggle_active_low(desc);

	ctx->ro_gpio = desc;

	return 0;
}
EXPORT_SYMBOL(mmc_gpiod_request_ro);

bool mmc_can_gpio_ro(struct mmc_host *host)
{
	struct mmc_gpio *ctx = host->slot.handler_priv;

	return ctx->ro_gpio ? true : false;
}
EXPORT_SYMBOL(mmc_can_gpio_ro);
