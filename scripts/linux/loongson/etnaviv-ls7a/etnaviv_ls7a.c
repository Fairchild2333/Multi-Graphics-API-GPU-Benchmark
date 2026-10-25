// SPDX-License-Identifier: GPL-2.0
/*
 * Binds the Vivante GC1000 GPU of the Loongson LS7A1000 bridge (PCI 0014:7a15)
 * to the etnaviv driver. Binding needs no change to etnaviv; drawing correctly
 * does (../ls7a-gpu-linux-7.1.patch: cached CPU mappings, VRAM import).
 *
 * etnaviv only finds GPU cores described in a device tree, and the LS7A1000's
 * GC1000 is a PCI function instead. Without a device tree, etnaviv's master
 * device "etnaviv" takes the names of its GPU cores from platform data, and
 * the core driver "etnaviv-gpu" matches a platform device of that name. This
 * driver creates both under the PCI device: the core gets the first 64 KiB of
 * BAR 0 (the GPU registers, as in Loongson's etnaviv PCI patches) and the PCI
 * interrupt.
 *
 * Two more things a device tree would give: the core driver's id table only
 * names "etnaviv-gpu,2d", so the core device is matched by driver_override;
 * and it requires a "core" clock, so a fixed 396 MHz clock (the GPU clock the
 * display driver reports for this bridge) is registered for it.
 */
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/sizes.h>

#define ETNAVIV_LS7A_CORE	"etnaviv-gpu"
#define ETNAVIV_LS7A_CLK_HZ	396000000

static const char *const etnaviv_ls7a_cores[] = { ETNAVIV_LS7A_CORE, NULL };

struct etnaviv_ls7a {
	struct clk_hw *clk;
	struct clk_lookup *clk_lookup;
	struct platform_device *core;
	struct platform_device *master;
};

static struct platform_device *etnaviv_ls7a_add_core(struct pci_dev *pdev)
{
	struct platform_device *core;
	struct resource res[2] = { };
	int ret;

	res[0].start = pci_resource_start(pdev, 0);
	res[0].end = res[0].start + SZ_64K - 1;
	res[0].flags = IORESOURCE_MEM;
	res[1].start = pdev->irq;
	res[1].end = pdev->irq;
	res[1].flags = IORESOURCE_IRQ;

	core = platform_device_alloc(ETNAVIV_LS7A_CORE, PLATFORM_DEVID_NONE);
	if (!core)
		return ERR_PTR(-ENOMEM);
	core->dev.parent = &pdev->dev;
	core->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	core->dev.dma_mask = &core->dev.coherent_dma_mask;
	ret = platform_device_add_resources(core, res, ARRAY_SIZE(res));
	if (!ret)
		ret = device_set_driver_override(&core->dev, ETNAVIV_LS7A_CORE);
	if (!ret)
		ret = platform_device_add(core);
	if (ret) {
		platform_device_put(core);
		return ERR_PTR(ret);
	}
	return core;
}

static int etnaviv_ls7a_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct platform_device_info info = { };
	struct etnaviv_ls7a *priv;
	int ret;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;
	pci_set_master(pdev);

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->clk = clk_hw_register_fixed_rate(&pdev->dev, "ls7a-gc1000-core", NULL, 0,
					       ETNAVIV_LS7A_CLK_HZ);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);
	priv->clk_lookup = clkdev_hw_create(priv->clk, "core", ETNAVIV_LS7A_CORE);
	if (!priv->clk_lookup) {
		clk_hw_unregister_fixed_rate(priv->clk);
		return -ENOMEM;
	}

	priv->core = etnaviv_ls7a_add_core(pdev);
	if (IS_ERR(priv->core)) {
		clkdev_drop(priv->clk_lookup);
		clk_hw_unregister_fixed_rate(priv->clk);
		return PTR_ERR(priv->core);
	}

	memset(&info, 0, sizeof(info));
	info.parent = &pdev->dev;
	info.name = "etnaviv";
	info.id = PLATFORM_DEVID_NONE;
	info.data = etnaviv_ls7a_cores;
	info.size_data = sizeof(etnaviv_ls7a_cores);
	info.dma_mask = DMA_BIT_MASK(32);
	priv->master = platform_device_register_full(&info);
	if (IS_ERR(priv->master)) {
		platform_device_unregister(priv->core);
		clkdev_drop(priv->clk_lookup);
		clk_hw_unregister_fixed_rate(priv->clk);
		return PTR_ERR(priv->master);
	}

	pci_set_drvdata(pdev, priv);
	dev_info(&pdev->dev, "GC1000 registers at %pR, irq %d, handed to etnaviv\n",
		 &priv->core->resource[0], pdev->irq);
	return 0;
}

static void etnaviv_ls7a_remove(struct pci_dev *pdev)
{
	struct etnaviv_ls7a *priv = pci_get_drvdata(pdev);

	platform_device_unregister(priv->master);
	platform_device_unregister(priv->core);
	clkdev_drop(priv->clk_lookup);
	clk_hw_unregister_fixed_rate(priv->clk);
}

static const struct pci_device_id etnaviv_ls7a_ids[] = {
	{ PCI_DEVICE(0x0014, 0x7a15) },
	{ }
};
MODULE_DEVICE_TABLE(pci, etnaviv_ls7a_ids);

static struct pci_driver etnaviv_ls7a_driver = {
	.name = "etnaviv-ls7a",
	.id_table = etnaviv_ls7a_ids,
	.probe = etnaviv_ls7a_probe,
	.remove = etnaviv_ls7a_remove,
};
module_pci_driver(etnaviv_ls7a_driver);

MODULE_DESCRIPTION("Hands the LS7A1000 GC1000 GPU to the etnaviv driver");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: etnaviv");
