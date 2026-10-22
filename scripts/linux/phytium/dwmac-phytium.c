// SPDX-License-Identifier: GPL-2.0
/*
 * Phytium DWMAC glue driver
 *
 * Copyright (C) 2026 Ricardo Pardini <ricardo@pardini.net>
 *
 * Glue layer for the Synopsys DesignWare MAC found on Phytium SoCs such as the
 * D2000, where the onboard GMAC is described through ACPI (_HID "PHYT0004").
 * Configuration comes from the ACPI _DSD, whose property names mirror the
 * snps,dwmac device-tree binding. The firmware exposes no clock controller, so
 * a fixed-rate clock is registered for the core to derive the MDC divider and
 * the PTP reference rate from.
 */

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/etherdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#include "stmmac.h"
#include "stmmac_platform.h"

#define PHYTIUM_DWMAC_DEFAULT_CLK_RATE	125000000	/* 125 MHz */
#define PHYTIUM_DWMAC_DEFAULT_FIFO_SIZE	0x10000

static void phytium_dwmac_clk_disable(void *data)
{
	clk_disable_unprepare(data);
}

static int phytium_dwmac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat;
	struct device *dev = &pdev->dev;
	struct fwnode_handle *fwnode;
	struct stmmac_resources res;
	struct acpi_device *adev;
	struct clk_hw *clk_hw;
	u64 clk_freq, uid;
	int ret;

	fwnode = dev_fwnode(dev);

	ret = stmmac_get_platform_resources(pdev, &res);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get platform resources\n");

	device_get_mac_address(dev, res.mac);

	plat = stmmac_plat_dat_alloc(dev);
	if (!plat)
		return -ENOMEM;

	/*
	 * Linux 7.0's stmmac_plat_dat_alloc() leaves dma_cfg to the glue (later
	 * kernels allocate it there); without this the probe below oopses.
	 */
	if (!plat->dma_cfg) {
		plat->dma_cfg = devm_kzalloc(dev, sizeof(*plat->dma_cfg),
					     GFP_KERNEL);
		if (!plat->dma_cfg)
			return -ENOMEM;
	}

	plat->mdio_bus_data = devm_kzalloc(dev, sizeof(*plat->mdio_bus_data),
					   GFP_KERNEL);
	if (!plat->mdio_bus_data)
		return -ENOMEM;

	ret = fwnode_get_phy_mode(fwnode);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to get PHY mode\n");
	plat->phy_interface = ret;

	plat->core_type = DWMAC_CORE_GMAC;
	plat->pmt = 1;

	/*
	 * Each MAC needs a distinct MDIO bus id, otherwise a second instance
	 * clashes on the bus name; derive it from the ACPI _UID.
	 */
	adev = ACPI_COMPANION(dev);
	if (adev && !acpi_dev_uid_to_integer(adev, &uid))
		plat->bus_id = uid;

	/* Queue 0 must be in DCB mode (MTL_QUEUE_AVB is the zero default). */
	plat->rx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;
	plat->tx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;
	plat->rx_sched_algorithm = MTL_RX_ALGORITHM_SP;
	plat->tx_sched_algorithm = MTL_TX_ALGORITHM_SP;

	plat->tx_fifo_size = PHYTIUM_DWMAC_DEFAULT_FIFO_SIZE;
	plat->rx_fifo_size = PHYTIUM_DWMAC_DEFAULT_FIFO_SIZE;

	device_property_read_u32(dev, "snps,pbl", &plat->dma_cfg->pbl);
	if (!plat->dma_cfg->pbl)
		plat->dma_cfg->pbl = DEFAULT_DMA_PBL;
	plat->dma_cfg->pblx8 = true;

	/*
	 * No clock controller is described in firmware, so register a
	 * fixed-rate clock as the MAC clock.
	 */
	if (fwnode_property_read_u64(fwnode, "clock-frequency", &clk_freq))
		clk_freq = PHYTIUM_DWMAC_DEFAULT_CLK_RATE;

	clk_hw = devm_clk_hw_register_fixed_rate(dev, dev_name(dev), NULL, 0,
						 clk_freq);
	if (IS_ERR(clk_hw))
		return dev_err_probe(dev, PTR_ERR(clk_hw),
				     "failed to register MAC clock\n");
	plat->stmmac_clk = clk_hw->clk;
	plat->clk_ptp_rate = clk_freq;

	ret = clk_prepare_enable(plat->stmmac_clk);
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable MAC clock\n");
	ret = devm_add_action_or_reset(dev, phytium_dwmac_clk_disable,
				       plat->stmmac_clk);
	if (ret)
		return ret;

	return devm_stmmac_pltfr_probe(pdev, plat, &res);
}

static const struct acpi_device_id phytium_dwmac_acpi_match[] = {
	{ "PHYT0004" },
	{ }
};
MODULE_DEVICE_TABLE(acpi, phytium_dwmac_acpi_match);

static struct platform_driver phytium_dwmac_driver = {
	.probe = phytium_dwmac_probe,
	.driver = {
		.name = "phytium-dwmac",
		.pm = &stmmac_pltfr_pm_ops,
		.acpi_match_table = phytium_dwmac_acpi_match,
	},
};
module_platform_driver(phytium_dwmac_driver);

MODULE_DESCRIPTION("Phytium DWMAC glue driver");
MODULE_AUTHOR("Ricardo Pardini <ricardo@pardini.net>");
MODULE_LICENSE("GPL");
