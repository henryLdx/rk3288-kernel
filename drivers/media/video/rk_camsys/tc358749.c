/*
 * rk3288_hdmiin.c
 *
 * Driver for rockchip rk3288 hdmiin
 * Copyright (C) 2015
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 *
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

struct ioctrl {
	int gpio;
	int active;
};

struct hdmi_in {
	struct device *dev;
	struct ioctrl io_power;
	struct ioctrl io_reset;
	struct ioctrl io_stanby;
	struct ioctrl io_int;
};

static struct hdmi_in *hdmi_in;

static int rk3288_hdmiin_probe(struct platform_device *pdev)
{
	struct device_node *np;
	enum of_gpio_flags flags;
	int ret;

	pr_info("%s for camera\n", __func__);
	np = pdev->dev.of_node;
	hdmi_in = devm_kzalloc(&pdev->dev,
				  sizeof(*hdmi_in), GFP_KERNEL);
	if (!hdmi_in) {
		dev_err(&pdev->dev, "rk3288 extern encoder device kmalloc fail!");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, hdmi_in);
	/* ----------------------------set power--------------------------*/
	hdmi_in->io_power.gpio = of_get_named_gpio_flags(np, "gpio-power",
							    0, &flags);
	if (!gpio_is_valid(hdmi_in->io_power.gpio)) {
		pr_err("invalid hdmiin->io_power.gpio: %d\n",
		       hdmi_in->io_power.gpio);
		goto failout;
		}
	ret = gpio_request(hdmi_in->io_power.gpio, "hdmiin-power-io");
	if (ret != 0) {
		pr_err("gpio_request hdmiin->io_power.gpio invalid: %d\n",
		       hdmi_in->io_power.gpio);
		goto failout;
		}
	hdmi_in->io_power.active = (flags & OF_GPIO_ACTIVE_LOW);
	gpio_direction_output(hdmi_in->io_power.gpio,
			      !(hdmi_in->io_power.active));
	gpio_set_value(hdmi_in->io_power.gpio,
		       !(hdmi_in->io_power.active));
	/*-----------------------------set power  end------------------------*/
	/* ----------------------------set stanby--------------------------*/
	hdmi_in->io_stanby.gpio = of_get_named_gpio_flags(np, "gpio-stanby",
							    0, &flags);
	if (!gpio_is_valid(hdmi_in->io_stanby.gpio)) {
		pr_err("invalid hdmiin->io_stanby.gpio: %d\n",
		       hdmi_in->io_stanby.gpio);
		goto failout1;
		}
	ret = gpio_request(hdmi_in->io_stanby.gpio, "hdmiin-stanby-io");
	if (ret != 0) {
		pr_err("gpio_request hdmiin->io_stanby.gpio invalid: %d\n",
		       hdmi_in->io_stanby.gpio);
		goto failout1;
		}
	hdmi_in->io_stanby.active = (flags & OF_GPIO_ACTIVE_LOW);
	gpio_direction_output(hdmi_in->io_stanby.gpio,
			      !(hdmi_in->io_stanby.active));
	gpio_set_value(hdmi_in->io_stanby.gpio,
		       !(hdmi_in->io_stanby.active));
	/*-----------------------------set stanby  end------------------------*/
	/* ----------------------------set reset--------------------------*/
	hdmi_in->io_reset.gpio = of_get_named_gpio_flags(np, "gpio-reset",
							    0, &flags);
	if (!gpio_is_valid(hdmi_in->io_reset.gpio)) {
		pr_err("invalid hdmiin->io_reset.gpio: %d\n",
		       hdmi_in->io_reset.gpio);
		goto failout2;
		}
	ret = gpio_request(hdmi_in->io_reset.gpio, "hdmiin-reset-io");
	if (ret != 0) {
		pr_err("gpio_request hdmiin->io_reset.gpio invalid: %d\n",
		       hdmi_in->io_reset.gpio);
		goto failout2;
		}
	hdmi_in->io_reset.active = (flags & OF_GPIO_ACTIVE_LOW);
	gpio_direction_output(hdmi_in->io_reset.gpio,
			      !(hdmi_in->io_reset.active));
	gpio_set_value(hdmi_in->io_reset.gpio,
		       !(hdmi_in->io_reset.active));
	/*-----------------------------set reset  end------------------------*/
	/* ----------------------------set interrupt--------------------------*/
	hdmi_in->io_int.gpio = of_get_named_gpio_flags(np, "gpio-int",
							    0, &flags);
	if (!gpio_is_valid(hdmi_in->io_int.gpio)) {
		pr_err("invalid hdmiin->io_int.gpio: %d\n",
		       hdmi_in->io_int.gpio);
		goto failout3;
		}
	ret = gpio_request(hdmi_in->io_int.gpio, "hdmiin-interrupt-io");
	if (ret != 0) {
		pr_err("gpio_request hdmiin->io_int.gpio invalid: %d\n",
		       hdmi_in->io_int.gpio);
		goto failout3;
		}
	gpio_direction_input(hdmi_in->io_int.gpio);
	/*-----------------------------set interrupt  end------------------------*/
	pr_info("%s: probe OK!\n", __func__);

	return 0;
failout3:
	gpio_free(hdmi_in->io_reset.gpio);
failout2:
	gpio_free(hdmi_in->io_stanby.gpio);
failout1:
	gpio_free(hdmi_in->io_power.gpio);
failout:
	return -1;
}

static void rk3288_hdmiin_shutdown(struct platform_device *pdev)
{
}

static const struct of_device_id rk3288_hdmiin_dt_ids[] = {
	{.compatible = "rockchip,rk3288-hdmiin",},
	{}
};

static struct platform_driver rk3288_hdmiin_driver = {
	.probe = rk3288_hdmiin_probe,
	.remove = NULL,
	.driver = {
		.name = "rk3288-hdmiin",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rk3288_hdmiin_dt_ids),
	},
	.shutdown = rk3288_hdmiin_shutdown,
};

static int __init rk3288_hdmiin_init(void)
{
	return platform_driver_register(&rk3288_hdmiin_driver);
}

static void __exit rk3288_hdmiin_exit(void)
{
	platform_driver_unregister(&rk3288_hdmiin_driver);
}

fs_initcall(rk3288_hdmiin_init);
module_exit(rk3288_hdmiin_exit);

MODULE_DESCRIPTION("ROCKCHIP RK3288 HDMI IN ");
MODULE_LICENSE("GPL");


