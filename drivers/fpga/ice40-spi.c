/*
 * FPGA Manager Driver for Lattice iCE40.
 *
 *  Copyright (c) 2016 Joel Holdsworth
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This driver adds support to the FPGA manager for configuring the SRAM of
 * Lattice iCE40 FPGAs through slave SPI.
 */

#include <linux/fpga/fpga-mgr.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/stringify.h>

#ifdef CONFIG_MACH_LGE
static bool ignore_cdone;
module_param(ignore_cdone, bool, 0644);
#endif

#define ICE40_SPI_MAX_SPEED 25000000 /* Hz */
#define ICE40_SPI_MIN_SPEED 1000000 /* Hz */

#define ICE40_SPI_RESET_DELAY 1 /* us (>200ns) */
#define ICE40_SPI_HOUSEKEEPING_DELAY 1200 /* us */

#define ICE40_SPI_NUM_ACTIVATION_BYTES DIV_ROUND_UP(49, 8)

struct ice40_fpga_priv {
	struct spi_device *dev;
	struct gpio_desc *reset;
	struct gpio_desc *cdone;
};

static enum fpga_mgr_states ice40_fpga_ops_state(struct fpga_manager *mgr)
{
	struct ice40_fpga_priv *priv = mgr->priv;

#ifdef CONFIG_MACH_LGE
	if (ignore_cdone || !priv->cdone)
		return FPGA_MGR_STATE_OPERATING;
#endif
	return gpiod_get_value(priv->cdone) ? FPGA_MGR_STATE_OPERATING :
		FPGA_MGR_STATE_UNKNOWN;
}

static int ice40_fpga_ops_write_init(struct fpga_manager *mgr,
				     struct fpga_image_info *info,
				     const char *buf, size_t count)
{
	struct ice40_fpga_priv *priv = mgr->priv;
	struct spi_device *dev = priv->dev;
	struct spi_message message;
	struct spi_transfer assert_cs_then_reset_delay = {
		.cs_change   = 1,
		.delay_usecs = ICE40_SPI_RESET_DELAY
	};
	struct spi_transfer housekeeping_delay_then_release_cs = {
		.delay_usecs = ICE40_SPI_HOUSEKEEPING_DELAY
	};
	int ret;

	if ((info->flags & FPGA_MGR_PARTIAL_RECONFIG)) {
		dev_err(&dev->dev,
			"Partial reconfiguration is not supported\n");
		return -ENOTSUPP;
	}

	/* Lock the bus, assert CRESET_B and SS_B and delay >200ns */
	spi_bus_lock(dev->master);

	gpiod_set_value(priv->reset, 1);

	spi_message_init(&message);
	spi_message_add_tail(&assert_cs_then_reset_delay, &message);
	ret = spi_sync_locked(dev, &message);

	/* Come out of reset */
	gpiod_set_value(priv->reset, 0);

	/* Abort if the chip-select failed */
	if (ret)
		goto fail;

	/* Check CDONE is de-asserted i.e. the FPGA is reset */
#ifdef CONFIG_MACH_LGE
	if (priv->cdone)
#endif
	if (gpiod_get_value(priv->cdone)) {
		dev_err(&dev->dev, "Device reset failed, CDONE is asserted\n");
		ret = -EIO;
#ifdef CONFIG_MACH_LGE
		if (!ignore_cdone)
#endif
		goto fail;
	}

	/* Wait for the housekeeping to complete, and release SS_B */
	spi_message_init(&message);
	spi_message_add_tail(&housekeeping_delay_then_release_cs, &message);
	ret = spi_sync_locked(dev, &message);

fail:
	spi_bus_unlock(dev->master);

	return ret;
}

static int ice40_fpga_ops_write(struct fpga_manager *mgr,
				const char *buf, size_t count)
{
	struct ice40_fpga_priv *priv = mgr->priv;

#ifdef CONFIG_MACH_LGE
	do {
		char tmp[128] = "";
		const char *p;

		if (buf[0] != 0xff || buf[1] != 0x00)
			break;

		p = buf + 2;
		while ((p - buf) < count && *p != 0xff) {
			snprintf(tmp, sizeof(tmp), "%s%s ", tmp, p);
			p += strlen(p) + 1;
		}
		dev_info(&priv->dev->dev, "%s\n", tmp);
	} while (0);
#endif

	return spi_write(priv->dev, buf, count);
}

static int ice40_fpga_ops_write_complete(struct fpga_manager *mgr,
					 struct fpga_image_info *info)
{
	struct ice40_fpga_priv *priv = mgr->priv;
	struct spi_device *dev = priv->dev;
	const u8 padding[ICE40_SPI_NUM_ACTIVATION_BYTES] = {0};

	/* Check CDONE is asserted */
#ifdef CONFIG_MACH_LGE
	if (priv->cdone)
#endif
	if (!gpiod_get_value(priv->cdone)) {
		dev_err(&dev->dev,
			"CDONE was not asserted after firmware transfer\n");
#ifdef CONFIG_MACH_LGE
		if (!ignore_cdone)
#endif
		return -EIO;
	}

	/* Send of zero-padding to activate the firmware */
#ifdef CONFIG_MACH_LGE
	return spi_write_then_read(dev, padding, sizeof(padding), NULL, 0);
#else
	return spi_write(dev, padding, sizeof(padding));
#endif
}

static const struct fpga_manager_ops ice40_fpga_ops = {
	.state = ice40_fpga_ops_state,
	.write_init = ice40_fpga_ops_write_init,
	.write = ice40_fpga_ops_write,
	.write_complete = ice40_fpga_ops_write_complete,
};

static int ice40_fpga_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ice40_fpga_priv *priv;
	struct fpga_manager *mgr;
	int ret;

	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = spi;

	/* Check board setup data. */
	if (spi->max_speed_hz > ICE40_SPI_MAX_SPEED) {
		dev_err(dev, "SPI speed is too high, maximum speed is "
			__stringify(ICE40_SPI_MAX_SPEED) "\n");
		return -EINVAL;
	}

	if (spi->max_speed_hz < ICE40_SPI_MIN_SPEED) {
		dev_err(dev, "SPI speed is too low, minimum speed is "
			__stringify(ICE40_SPI_MIN_SPEED) "\n");
		return -EINVAL;
	}

	if (spi->mode & SPI_CPHA) {
		dev_err(dev, "Bad SPI mode, CPHA not supported\n");
		return -EINVAL;
	}

	/* Set up the GPIOs */
	priv->cdone = devm_gpiod_get(dev, "cdone", GPIOD_IN);
	if (IS_ERR(priv->cdone)) {
		ret = PTR_ERR(priv->cdone);
		dev_err(dev, "Failed to get CDONE GPIO: %d\n", ret);
#ifdef CONFIG_MACH_LGE
		priv->cdone = NULL;
#else
		return ret;
#endif
	}

#ifdef CONFIG_MACH_LGE
	priv->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
#else
	priv->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
#endif
	if (IS_ERR(priv->reset)) {
		ret = PTR_ERR(priv->reset);
		dev_err(dev, "Failed to get CRESET_B GPIO: %d\n", ret);
		return ret;
	}

	mgr = fpga_mgr_create(dev, "Lattice iCE40 FPGA Manager",
			      &ice40_fpga_ops, priv);
	if (!mgr)
		return -ENOMEM;

	spi_set_drvdata(spi, mgr);

	ret = fpga_mgr_register(mgr);
	if (ret)
		fpga_mgr_free(mgr);

	return ret;
}

static int ice40_fpga_remove(struct spi_device *spi)
{
	struct fpga_manager *mgr = spi_get_drvdata(spi);

	fpga_mgr_unregister(mgr);

	return 0;
}

static const struct of_device_id ice40_fpga_of_match[] = {
	{ .compatible = "lattice,ice40-fpga-mgr", },
	{},
};
MODULE_DEVICE_TABLE(of, ice40_fpga_of_match);

static struct spi_driver ice40_fpga_driver = {
	.probe = ice40_fpga_probe,
	.remove = ice40_fpga_remove,
	.driver = {
		.name = "ice40spi",
		.of_match_table = of_match_ptr(ice40_fpga_of_match),
	},
};

module_spi_driver(ice40_fpga_driver);

MODULE_AUTHOR("Joel Holdsworth <joel@airwebreathe.org.uk>");
MODULE_DESCRIPTION("Lattice iCE40 FPGA Manager");
MODULE_LICENSE("GPL v2");
