// SPDX-License-Identifier: GPL-2.0-only
/* ESP32-S31 GPSPI master. PIO, mode 0, GPIO chip selects, 1/4-bit TX. */
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/spi/spi.h>

#define S31_SPI_CMD		0x00
#define S31_SPI_UPDATE		BIT(23)
#define S31_SPI_USR		BIT(24)
#define S31_SPI_CTRL		0x08
#define S31_SPI_CLOCK		0x0c
#define S31_SPI_CLK_L		GENMASK(5, 0)
#define S31_SPI_CLK_H		GENMASK(11, 6)
#define S31_SPI_CLK_N		GENMASK(17, 12)
#define S31_SPI_CLK_PRE		GENMASK(21, 18)
#define S31_SPI_USER		0x10
#define S31_SPI_QUAD		BIT(13)
#define S31_SPI_MOSI		BIT(27)
#define S31_SPI_MISO		BIT(28)
#define S31_SPI_USER1		0x14
#define S31_SPI_DLEN		0x1c
#define S31_SPI_MISC		0x20
#define S31_SPI_CS_DISABLE	GENMASK(5, 0)
#define S31_SPI_DMA_CONF		0x30
#define S31_SPI_INT_ENA		0x34
#define S31_SPI_W0		0x98
#define S31_SPI_SLAVE		0xe0
#define S31_SPI_CLK_GATE		0xe8
#define S31_SPI_CLK_ENABLE	(BIT(0) | BIT(1))
#define S31_SPI_FIFO_SIZE	64

struct esp32s31_spi {
	void __iomem *base;
	unsigned long rate;
};

static int esp32s31_spi_transfer_one(struct spi_controller *ctlr,
				   struct spi_device *spi,
				   struct spi_transfer *xfer)
{
	struct esp32s31_spi *es = spi_controller_get_devdata(ctlr);
	const u8 *tx = xfer->tx_buf;
	u8 *rx = xfer->rx_buf;
	unsigned int pre, n, offset, count, i;
	u32 clock, user, value;
	int ret;

	/* Round the divisor up: never exceed the peripheral's requested rate. */
	for (pre = 1; pre <= 16; pre++) {
		n = DIV_ROUND_UP(es->rate, (unsigned long)xfer->speed_hz * pre);
		if (n <= 64)
			break;
	}
	if (pre > 16)
		return -EINVAL;
	n = max(n, 2U);
	clock = FIELD_PREP(S31_SPI_CLK_L, n - 1) |
		FIELD_PREP(S31_SPI_CLK_H, n / 2 - 1) |
		FIELD_PREP(S31_SPI_CLK_N, n - 1) |
		FIELD_PREP(S31_SPI_CLK_PRE, pre - 1);
	writel(clock, es->base + S31_SPI_CLOCK);
	xfer->effective_speed_hz = es->rate / (pre * n);
	user = tx ? S31_SPI_MOSI : S31_SPI_MISO;
	if (xfer->tx_nbits == SPI_NBITS_QUAD)
		user |= S31_SPI_QUAD;
	writel(user, es->base + S31_SPI_USER);

	for (offset = 0; offset < xfer->len; offset += count) {
		count = min_t(unsigned int, xfer->len - offset, S31_SPI_FIFO_SIZE);
		if (tx) {
			for (i = 0; i < count; i += sizeof(value)) {
				value = 0;
				memcpy(&value, tx + offset + i,
				       min_t(unsigned int, sizeof(value), count - i));
				writel(value, es->base + S31_SPI_W0 + i);
			}
		}
		writel(count * 8 - 1, es->base + S31_SPI_DLEN);
		writel(S31_SPI_UPDATE, es->base + S31_SPI_CMD);
		ret = readl_poll_timeout(es->base + S31_SPI_CMD, value,
					 !(value & S31_SPI_UPDATE), 0, 10000);
		if (ret)
			return ret;
		writel(S31_SPI_USR, es->base + S31_SPI_CMD);
		ret = readl_poll_timeout(es->base + S31_SPI_CMD, value,
					 !(value & S31_SPI_USR), 0, 10000);
		if (ret)
			return ret;
		if (rx) {
			for (i = 0; i < count; i += sizeof(value)) {
				value = readl(es->base + S31_SPI_W0 + i);
				memcpy(rx + offset + i, &value,
				       min_t(unsigned int, sizeof(value), count - i));
			}
		}
		cond_resched();
	}
	return 0;
}

static int esp32s31_spi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct spi_controller *ctlr;
	struct esp32s31_spi *es;
	struct reset_control *reset;
	struct clk *core;
	int ret;

	ctlr = devm_spi_alloc_host(dev, sizeof(*es));
	if (!ctlr)
		return -ENOMEM;
	es = spi_controller_get_devdata(ctlr);
	es->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(es->base))
		return PTR_ERR(es->base);

	/* Keep both clocks enabled for the lifetime of the controller. */
	core = devm_clk_get_enabled(dev, "apb");
	if (IS_ERR(core))
		return dev_err_probe(dev, PTR_ERR(core), "APB clock\n");
	core = devm_clk_get_enabled(dev, "core");
	if (IS_ERR(core))
		return dev_err_probe(dev, PTR_ERR(core), "core clock\n");
	es->rate = clk_get_rate(core);
	if (!es->rate)
		return -EINVAL;

	reset = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(reset))
		return dev_err_probe(dev, PTR_ERR(reset), "reset\n");
	ret = reset_control_assert(reset);
	if (ret)
		return ret;
	ret = reset_control_deassert(reset);
	if (ret)
		return ret;

	writel(S31_SPI_CLK_ENABLE, es->base + S31_SPI_CLK_GATE);
	writel(0, es->base + S31_SPI_SLAVE);
	writel(0, es->base + S31_SPI_CTRL);
	writel(0, es->base + S31_SPI_USER);
	writel(0, es->base + S31_SPI_USER1);
	writel(0, es->base + S31_SPI_DMA_CONF);
	writel(0, es->base + S31_SPI_INT_ENA);
	writel(S31_SPI_CS_DISABLE, es->base + S31_SPI_MISC);

	ctlr->dev.of_node = dev->of_node;
	ctlr->mode_bits = SPI_TX_QUAD;
	ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
	ctlr->flags = SPI_CONTROLLER_HALF_DUPLEX;
	ctlr->use_gpio_descriptors = true;
	ctlr->num_chipselect = 1;
	ctlr->max_speed_hz = es->rate / 2;
	ctlr->min_speed_hz = DIV_ROUND_UP(es->rate, 16 * 64);
	ctlr->transfer_one = esp32s31_spi_transfer_one;

	return devm_spi_register_controller(dev, ctlr);
}

static const struct of_device_id esp32s31_spi_of_match[] = {
	{ .compatible = "esp,esp32s31-spi" },
	{}
};
MODULE_DEVICE_TABLE(of, esp32s31_spi_of_match);

static struct platform_driver esp32s31_spi_driver = {
	.probe = esp32s31_spi_probe,
	.driver = {
		.name = "esp32s31-spi",
		.of_match_table = esp32s31_spi_of_match,
	},
};
module_platform_driver(esp32s31_spi_driver);

MODULE_DESCRIPTION("ESP32-S31 PIO SPI master");
MODULE_LICENSE("GPL");
