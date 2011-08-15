/* linux/arch/arm/mach-s5pv210/setup-fb.c
 *
 * Copyright (c) 2009 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * Base FIMD controller configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <plat/fb.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>
#include <linux/io.h>
#include <mach/map.h>
#include <mach/pd.h>
#include <mach/gpio-bank.h>

struct platform_device; /* don't need the contents */

void s3cfb_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
	writel(0x2, S5P_MDNIE_SEL);

	/* drive strength to max */
	writel(0xffffffff, S5PV210_GPF0_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF1_BASE + 0xc);
	writel(0xffffffff, S5PV210_GPF2_BASE + 0xc);
	writel(0x000000ff, S5PV210_GPF3_BASE + 0xc);
}

int s3cfb_clk_on(struct platform_device *pdev, struct clk **s3cfb_clk)
{
	struct clk *sclk = NULL;
	struct clk *mout_fimd = NULL, *mout_mpll = NULL;
	u32 rate = 0;
	int ret;

	sclk = clk_get(&pdev->dev, "sclk_fimd");
	if (IS_ERR(sclk)) {
		dev_err(&pdev->dev, "failed to get sclk for fimd\n");
		goto err_clk1;
	}

#if defined(CONFIG_S5PV210_SCLKFIMD_USE_VPLL)
	mout_mpll = clk_get(&pdev->dev, "mout_vpll");
#else
	mout_mpll = clk_get(&pdev->dev, "mout_mpll");
#endif
	if (IS_ERR(mout_mpll)) {
		dev_err(&pdev->dev, "failed to get mout_mpll\n");
		goto err_clk1;
	}

	mout_fimd = clk_get(&pdev->dev, "mout_fimd");
	if (IS_ERR(mout_fimd)) {
		dev_err(&pdev->dev,
				"failed to get mout_fimd\n");
		goto err_clk2;
	}

	clk_set_parent(sclk, mout_fimd);
	clk_set_parent(mout_fimd, mout_mpll);

	rate = clk_round_rate(sclk, 166750000);
	dev_dbg(&pdev->dev, "set fimd sclk rate to %d\n", rate);

	if (!rate)
		rate = 166750000;

#if defined(CONFIG_MACH_S5PC110_P1)
	#if defined(CONFIG_TARGET_PCLK_44_46)
		rate = 45000000;
	#elif defined(CONFIG_TARGET_PCLK_47_6)
		//P1_ATT PCLK -> 47.6MHz
		rate = 48000000;
	#else
		rate = 54000000;
	#endif
#endif

	clk_set_rate(sclk, rate);
	dev_dbg(&pdev->dev, "set fimd sclk rate to %d\n", rate);

	clk_put(mout_mpll);
	clk_put(mout_fimd);

#if defined(CONFIG_MACH_S5PC110_P1)
	{
	struct clk * sclk_mdnie;
	struct clk * sclk_mdnie_pwm;
	sclk_mdnie = clk_get(&pdev->dev, "sclk_mdnie");
	if (IS_ERR(sclk)) {
		dev_err(&pdev->dev, "failed to get sclk for mdnie\n");
		}
	else
		{
	#if defined(CONFIG_TARGET_PCLK_44_46)
		clk_set_rate(sclk_mdnie, 45*1000000);
	#elif defined(CONFIG_TARGET_PCLK_47_6)
		//P1_ATT PCLK -> 47.6MHz
		clk_set_rate(sclk_mdnie, 48*1000000);
	#else
		clk_set_rate(sclk_mdnie, 54*1000000);
	#endif
		clk_put(sclk_mdnie);
		}
	sclk_mdnie_pwm = clk_get(&pdev->dev, "sclk_mdnie_pwm");
	if (IS_ERR(sclk)) {
		dev_err(&pdev->dev, "failed to get sclk for mdnie pwm\n");
		}
	else
		{
		clk_set_rate(sclk_mdnie_pwm, 2400*1000);		// mdnie pwm need to 24Khz*100
		clk_put(sclk_mdnie_pwm);
		}
	}
#endif

	ret = s5pv210_pd_enable("fimd_pd");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable fimd power domain\n");
		goto err_clk2;
	}

	clk_enable(sclk);

	*s3cfb_clk = sclk;

	return 0;

err_clk2:
	clk_put(mout_mpll);

err_clk1:
	clk_put(sclk);

	return -EINVAL;
}

int s3cfb_clk_off(struct platform_device *pdev, struct clk **clk)
{
	int ret;

	clk_disable(*clk);
	clk_put(*clk);

	*clk = NULL;
	ret = s5pv210_pd_disable("fimd_pd");
	if (ret < 0)
		dev_err(&pdev->dev, "failed to disable fimd power domain\n");

	return 0;
}

void s3cfb_get_clk_name(char *clk_name)
{
	strcpy(clk_name, "sclk_fimd");
}
#ifdef CONFIG_FB_S3C_LTE480WV
int s3cfb_backlight_onoff(struct platform_device *pdev, int onoff)
{
	int err;

	err = gpio_request(S5PV210_GPD0(3), "GPD0");

	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
			"lcd backlight control\n");
		return err;
	}

	if (onoff) {
		gpio_direction_output(S5PV210_GPD0(3), 1);
		/* 2009.12.28 by icarus : added for PWM backlight */
		s3c_gpio_cfgpin(S5PV210_GPD0(3), S5PV210_GPD_0_3_TOUT_3);

	}
	else {
		gpio_direction_output(S5PV210_GPD0(3), 0);
	}
	gpio_free(S5PV210_GPD0(3));
	return 0;
}

int s3cfb_reset_lcd(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_GPH0(6), "GPH0");
	if (err) {
		printk(KERN_ERR "failed to request GPH0 for "
			"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPH0(6), 1);
	mdelay(100);

	gpio_set_value(S5PV210_GPH0(6), 0);
	mdelay(10);

	gpio_set_value(S5PV210_GPH0(6), 1);
	mdelay(10);

	gpio_free(S5PV210_GPH0(6));

	return 0;
}
#elif defined(CONFIG_FB_S3C_HT101HD1)
int s3cfb_backlight_on(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_GPB(2), "GPB");
	if (err) {
		printk(KERN_ERR "failed to request GPB for "
			"lcd backlight control\n");
		return err;
	}

#ifdef CONFIG_TYPE_PROTO3
	err = gpio_request(S5PV210_GPD0(1), "GPD0");
	if (err) {
		printk(KERN_ERR "failed to request GPD0 for "
			"lcd backlight control\n");
		return err;
	}
#endif

	gpio_direction_output(S5PV210_GPB(2), 1); /* LED_EN (SPI1_MOSI) */

#ifdef CONFIG_TYPE_PROTO3
	/* LCD_PWR_EN is only for Proto3 */
	gpio_direction_output(S5PV210_GPD0(1), 1);
	mdelay(10);
#endif

	gpio_free(S5PV210_GPB(2));
#ifdef CONFIG_TYPE_PROTO3
	gpio_free(S5PV210_GPD0(1));
#endif

	return 0;
}

int s3cfb_reset_lcd(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_GPH0(1), "GPH0");
	if (err) {
		printk(KERN_ERR "failed to request GPH0 for "
			"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPH0(1), 1);

	gpio_set_value(S5PV210_GPH0(1), 0);

	gpio_set_value(S5PV210_GPH0(1), 1);

	gpio_free(S5PV210_GPH0(1));

	return 0;
}
#endif
