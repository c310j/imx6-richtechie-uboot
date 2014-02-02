/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/mx6.h>
#include <asm/arch/mx6_pins.h>
#include <asm/arch/mx6dl_pins.h>
#include <asm/arch/iomux-v3.h>
#include <asm/arch/regs-anadig.h>
#include <asm/errno.h>
#ifdef CONFIG_MXC_FEC
#include <miiphy.h>
#endif
#if defined(CONFIG_VIDEO_MX5)
#include <linux/list.h>
#include <linux/fb.h>
#include <linux/mxcfb.h>
#include <ipu.h>
#include <lcd.h>
#endif

#ifdef CONFIG_IMX_ECSPI
#include <imx_spi.h>
#endif

#if CONFIG_I2C_MXC
#include <i2c.h>
#endif

#ifdef CONFIG_CMD_MMC
#include <mmc.h>
#include <fsl_esdhc.h>
#endif

#ifdef CONFIG_ARCH_MMU
#include <asm/mmu.h>
#include <asm/arch/mmu.h>
#endif

#ifdef CONFIG_CMD_CLOCK
#include <asm/clock.h>
#endif

#ifdef CONFIG_CMD_IMXOTP
#include <imx_otp.h>
#endif

#ifdef CONFIG_MXC_GPIO
#include <asm/gpio.h>
#include <asm/arch/gpio.h>
#endif

#ifdef CONFIG_ANDROID_RECOVERY
#include <recovery.h>
#endif
DECLARE_GLOBAL_DATA_PTR;

#include <asm/imx_pwm.h>

static enum boot_device boot_dev;

#define GPIO_REVB_POWER_KEY IMX_GPIO_NR(1, 27)
#define USB_OTG_PWR IMX_GPIO_NR(4, 15)
#define USB_H1_POWER IMX_GPIO_NR(4, 14)

#define HDMIDONGLE_CHG_SYS_ON	IMX_GPIO_NR(2, 6)

#ifdef CONFIG_VIDEO_MX5
extern unsigned char fsl_bmp_600x400[];
extern int fsl_bmp_600x400_size;
extern int g_ipu_hw_rev;

#if defined(CONFIG_BMP_8BPP)
unsigned short colormap[256];
#elif defined(CONFIG_BMP_16BPP)
unsigned short colormap[65536];
#else
unsigned short colormap[16777216];
#endif

static int di = 1;

extern int ipuv3_fb_init(struct fb_videomode *mode, int di,
			int interface_pix_fmt,
			ipu_di_clk_parent_t di_clk_parent,
			int di_clk_val);

static struct fb_videomode lvds_xga = {
	 "XGA", 60, 1024, 768, 15385, 220, 40, 21, 7, 60, 10,
	 FB_SYNC_EXT,
	 FB_VMODE_NONINTERLACED,
	 0,
};

vidinfo_t panel_info;
#endif


void func_27800f08(void)
{
	u32 reg;

	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_CLE__GPIO_6_7);
	//IOMUX_PAD(0x06BC, 0x02D4, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 6bc 2d4

	reg = readl(GPIO6_BASE_ADDR + 4);
	reg |= (1 << 7);
	writel(reg, GPIO6_BASE_ADDR + 4);

	reg = readl(GPIO6_BASE_ADDR + 0);
	reg &= ~(1 << 7);
	writel(reg, GPIO6_BASE_ADDR + 0);

	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_CS1__GPIO_2_24);
	//IOMUX_PAD(0x0410, 0x00FC, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 410 0fc

	reg = readl(GPIO2_BASE_ADDR + 4);
	reg |= (1 << 24);
	writel(reg, GPIO2_BASE_ADDR + 4);

	reg = readl(GPIO2_BASE_ADDR + 0);
	reg &= ~(1 << 24);
	writel(reg, GPIO2_BASE_ADDR + 0);
}

static inline void setup_boot_device(void)
{
	uint soc_sbmr = readl(SRC_BASE_ADDR + 0x4);
	uint bt_mem_ctl = (soc_sbmr & 0x000000FF) >> 4 ;
	uint bt_mem_type = (soc_sbmr & 0x00000008) >> 3;

	switch (bt_mem_ctl) {
	case 0x0:
		if (bt_mem_type)
			boot_dev = ONE_NAND_BOOT;
		else
			boot_dev = WEIM_NOR_BOOT;
		break;
	case 0x2:
			boot_dev = SATA_BOOT;
		break;
	case 0x3:
		if (bt_mem_type)
			boot_dev = SPI_NOR_BOOT; //I2C_BOOT;
		else
			boot_dev = I2C_BOOT; //SPI_NOR_BOOT;
		break;
	case 0x4:
	case 0x5:
		boot_dev = SD_BOOT;
		break;
	case 0x6:
	case 0x7:
		boot_dev = MMC_BOOT;
		break;
	case 0x8 ... 0xf:
		boot_dev = NAND_BOOT;
		break;
	default:
		boot_dev = UNKNOWN_BOOT;
		break;
	}
}

enum boot_device get_boot_device(void)
{
	return boot_dev;
}

u32 get_board_rev(void)
{
	return fsl_system_rev;
}

#ifdef CONFIG_ARCH_MMU
void board_mmu_init(void)
{
	unsigned long ttb_base = PHYS_SDRAM_1 + 0x4000;
	unsigned long i;

	/*
	* Set the TTB register
	*/
	asm volatile ("mcr  p15,0,%0,c2,c0,0" : : "r"(ttb_base) /*:*/);

	/*
	* Set the Domain Access Control Register
	*/
	i = ARM_ACCESS_DACR_DEFAULT;
	asm volatile ("mcr  p15,0,%0,c3,c0,0" : : "r"(i) /*:*/);

	/*
	* First clear all TT entries - ie Set them to Faulting
	*/
	memset((void *)ttb_base, 0, ARM_FIRST_LEVEL_PAGE_TABLE_SIZE);
	/* Actual   Virtual  Size   Attributes          Function */
	/* Base     Base     MB     cached? buffered?  access permissions */
	/* xxx00000 xxx00000 */
	X_ARM_MMU_SECTION(0x000, 0x000, 0x001,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* ROM, 1M */
	X_ARM_MMU_SECTION(0x001, 0x001, 0x008,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* 8M */
	X_ARM_MMU_SECTION(0x009, 0x009, 0x001,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* IRAM */
	X_ARM_MMU_SECTION(0x00A, 0x00A, 0x0F6,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW); /* 246M */

	/* 2 GB memory starting at 0x10000000, only map 1.875 GB */
	X_ARM_MMU_SECTION(0x100, 0x100, 0x780,
			ARM_CACHEABLE, ARM_BUFFERABLE,
			ARM_ACCESS_PERM_RW_RW);
	/* uncached alias of the same 1.875 GB memory */
	X_ARM_MMU_SECTION(0x100, 0x880, 0x780,
			ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			ARM_ACCESS_PERM_RW_RW);

	/* Enable MMU */
	MMU_ON();
}
#endif

#ifdef CONFIG_DWC_AHSATA

#define ANATOP_PLL_LOCK                 0x80000000
#define ANATOP_PLL_ENABLE_MASK          0x00002000
#define ANATOP_PLL_BYPASS_MASK          0x00010000
#define ANATOP_PLL_LOCK                 0x80000000
#define ANATOP_PLL_PWDN_MASK            0x00001000
#define ANATOP_PLL_HOLD_RING_OFF_MASK   0x00000800
#define ANATOP_SATA_CLK_ENABLE_MASK     0x00100000

int setup_sata(void)
{
	u32 reg = 0;
	s32 timeout = 100000;

	/* Enable sata clock */
	reg = readl(CCM_BASE_ADDR + 0x7c); /* CCGR5 */
	reg |= 0x30;
	writel(reg, CCM_BASE_ADDR + 0x7c);

	/* Enable PLLs */
	reg = readl(ANATOP_BASE_ADDR + 0xe0); /* ENET PLL */
	reg &= ~ANATOP_PLL_PWDN_MASK;
	writel(reg, ANATOP_BASE_ADDR + 0xe0);
	reg |= ANATOP_PLL_ENABLE_MASK;
	while (timeout--) {
		if (readl(ANATOP_BASE_ADDR + 0xe0) & ANATOP_PLL_LOCK)
			break;
	}
	if (timeout <= 0)
		return -1;
	reg &= ~ANATOP_PLL_BYPASS_MASK;
	writel(reg, ANATOP_BASE_ADDR + 0xe0);
	reg |= ANATOP_SATA_CLK_ENABLE_MASK;
	writel(reg, ANATOP_BASE_ADDR + 0xe0);

	/* Enable sata phy */
	reg = readl(IOMUXC_BASE_ADDR + 0x34); /* GPR13 */

	reg &= ~0x07ffffff;
	/*
	 * rx_eq_val_0 = 5 [26:24]
	 * los_lvl = 0x12 [23:19]
	 * rx_dpll_mode_0 = 0x3 [18:16]
	 * mpll_ss_en = 0x0 [14]
	 * tx_atten_0 = 0x4 [13:11]
	 * tx_boost_0 = 0x0 [10:7]
	 * tx_lvl = 0x11 [6:2]
	 * mpll_ck_off_b = 0x1 [1]
	 * tx_edgerate_0 = 0x0 [0]
	 * */
	reg |= 0x59124c6;
	writel(reg, IOMUXC_BASE_ADDR + 0x34);

	return 0;
}
#endif

int dram_init(void)
{
	/*
	 * Switch PL301_FAST2 to DDR Dual-channel mapping
	 * however this block the boot up, temperory redraw
	 */
	/*
	 * u32 reg = 1;
	 * writel(reg, GPV0_BASE_ADDR);
	 */

	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

static void setup_uart(void)
{
#if defined CONFIG_MX6Q
	/* UART4 TXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL0__UART4_TXD);

	/* UART4 RXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW0__UART4_RXD);
#elif defined CONFIG_MX6DL
	/* UART4 TXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL0__UART4_TXD);

	/* UART4 RXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW0__UART4_RXD);
#endif
}

#ifdef CONFIG_VIDEO_MX5
static struct pwm_device pwm = { //2782ad68
		0, //unsigned long mmio_base;
		0, //unsigned int pwm_id;
		0, //int pwmo_invert;
		0, //void (*enable_pwm_pad)(void);
		0, //void (*disable_pwm_pad)(void);
		0, //void (*enable_pwm_clk)(void);
		0, //void (*disable_pwm_clk)(void);
};

void setup_lvds_poweron(void)
{
	int reg;

	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_D4__GPIO_2_4);
	//IOMUX_PAD(0x06F4, 0x030C, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 6f4 30c
	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_D5__GPIO_2_5);
	//IOMUX_PAD(0x06F8, 0x0310, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 6f8 310
	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_D7__GPIO_2_7);
	//IOMUX_PAD(0x0700, 0x0318, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 700 318
	mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_8__GPIO_1_8);
	//IOMUX_PAD(0x0614, 0x0244, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 614 244
	mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_9__PWM1_PWMO);
	//IOMUX_PAD(0x05F8, 0x0228, 4, 0x0000, 0, 0) | MUX_PAD_CTRL(MX6Q_HIGH_DRV) = 037162 4 000 5f8 228

	reg = readl(GPIO1_BASE_ADDR + 4);
	reg |= (1 << 8);
	writel(reg, GPIO1_BASE_ADDR + 4);

	reg = readl(GPIO2_BASE_ADDR + 4);
	reg |= 0xb0;
	writel(reg, GPIO2_BASE_ADDR + 4);

	reg = readl(GPIO2_BASE_ADDR + 0);
	reg &= ~(1 << 4);
	writel(reg, GPIO2_BASE_ADDR + 0);

	writel(reg | (1 << 7), GPIO2_BASE_ADDR + 0);

	udelay(60000);
	udelay(60000);
	udelay(60000);
	udelay(20000);

	writel(reg | 0xA0, GPIO2_BASE_ADDR + 0);

	udelay(10000);

	imx_pwm_config(pwm, 1500000/*duty_ns*/, 4900000/*period_ns*/);
	imx_pwm_enable(pwm);

	writel(reg | 0xA0, GPIO1_BASE_ADDR + 0);
	udelay(500);
	writel(reg | 0x1A0, GPIO1_BASE_ADDR + 0);
}
#endif

#ifdef CONFIG_I2C_MXC
#define I2C1_SDA_GPIO5_26_BIT_MASK  (1 << 26)
#define I2C1_SCL_GPIO5_27_BIT_MASK  (1 << 27)
#define I2C2_SCL_GPIO4_12_BIT_MASK  (1 << 12)
#define I2C2_SDA_GPIO4_13_BIT_MASK  (1 << 13)
#define I2C3_SCL_GPIO1_3_BIT_MASK   (1 << 3)
#define I2C3_SDA_GPIO1_6_BIT_MASK   (1 << 6)

static void setup_i2c(unsigned int module_base)
{
	unsigned int reg;

	switch (module_base) {
	case I2C1_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* i2c1 SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_CSI0_DAT8__I2C1_SDA);

		/* i2c1 SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_CSI0_DAT9__I2C1_SCL);
#elif defined CONFIG_MX6DL
		/* i2c1 SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_CSI0_DAT8__I2C1_SDA);
		/* i2c1 SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_CSI0_DAT9__I2C1_SCL);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0xC0;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	case I2C2_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* i2c2 SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW3__I2C2_SDA);

		/* i2c2 SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL3__I2C2_SCL);
#elif defined CONFIG_MX6DL
		/* i2c2 SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW3__I2C2_SDA);

		/* i2c2 SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL3__I2C2_SCL);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0x300;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	case I2C3_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* GPIO_3 for I2C3_SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_3__I2C3_SCL);
		/* GPIO_6 for I2C3_SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_6__I2C3_SDA);

#elif defined CONFIG_MX6DL
		/* GPIO_3 for I2C3_SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_3__I2C3_SCL);
		/* GPIO_6 for I2C3_SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_6__I2C3_SDA);
#endif
		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0xC00;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	default:
		printf("Invalid I2C base: 0x%x\n", module_base);
		break;
	}
}
/* Note: udelay() is not accurate for i2c timing */
static void __udelay(int time)
{
	int i, j;

	for (i = 0; i < time; i++) {
		for (j = 0; j < 200; j++) {
			asm("nop");
			asm("nop");
		}
	}
}
static void mx6q_i2c_gpio_scl_direction(int bus, int output)
{
	u32 reg;

	switch (bus) {
	case 1:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_CSI0_DAT9__GPIO_5_27);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_CSI0_DAT9__GPIO_5_27);
#endif
		reg = readl(GPIO5_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C1_SCL_GPIO5_27_BIT_MASK;
		else
			reg &= ~I2C1_SCL_GPIO5_27_BIT_MASK;
		writel(reg, GPIO5_BASE_ADDR + GPIO_GDIR);
		break;
	case 2:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL3__GPIO_4_12);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL3__GPIO_4_12);
#endif
		reg = readl(GPIO4_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C2_SCL_GPIO4_12_BIT_MASK;
		else
			reg &= ~I2C2_SCL_GPIO4_12_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_GDIR);
		break;
	case 3:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_3__GPIO_1_3);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_3__GPIO_1_3);
#endif
		reg = readl(GPIO1_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C3_SCL_GPIO1_3_BIT_MASK;
		else
			reg &= I2C3_SCL_GPIO1_3_BIT_MASK;
		writel(reg, GPIO1_BASE_ADDR + GPIO_GDIR);
		break;
	}
}

/* set 1 to output, sent 0 to input */
static void mx6q_i2c_gpio_sda_direction(int bus, int output)
{
	u32 reg;

	switch (bus) {
	case 1:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_CSI0_DAT8__GPIO_5_26);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_CSI0_DAT8__GPIO_5_26);
#endif
		reg = readl(GPIO5_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C1_SDA_GPIO5_26_BIT_MASK;
		else
			reg &= ~I2C1_SDA_GPIO5_26_BIT_MASK;
		writel(reg, GPIO5_BASE_ADDR + GPIO_GDIR);
		break;
	case 2:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW3__GPIO_4_13);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW3__GPIO_4_13);
#endif
		reg = readl(GPIO4_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C2_SDA_GPIO4_13_BIT_MASK;
		else
			reg &= ~I2C2_SDA_GPIO4_13_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_GDIR);
	case 3:
#if defined CONFIG_MX6Q
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_6__GPIO_1_6);
#elif defined CONFIG_MX6DL
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_6__GPIO_1_6);
#endif
		reg = readl(GPIO1_BASE_ADDR + GPIO_GDIR);
		if (output)
			reg |= I2C3_SDA_GPIO1_6_BIT_MASK;
		else
			reg &= ~I2C3_SDA_GPIO1_6_BIT_MASK;
		writel(reg, GPIO1_BASE_ADDR + GPIO_GDIR);
	default:
		break;
	}
}

/* set 1 to high 0 to low */
static void mx6q_i2c_gpio_scl_set_level(int bus, int high)
{
	u32 reg;

	switch (bus) {
	case 1:
		reg = readl(GPIO5_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C1_SCL_GPIO5_27_BIT_MASK;
		else
			reg &= ~I2C1_SCL_GPIO5_27_BIT_MASK;
		writel(reg, GPIO5_BASE_ADDR + GPIO_DR);
		break;
	case 2:
		reg = readl(GPIO4_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C2_SCL_GPIO4_12_BIT_MASK;
		else
			reg &= ~I2C2_SCL_GPIO4_12_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_DR);
		break;
	case 3:
		reg = readl(GPIO1_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C3_SCL_GPIO1_3_BIT_MASK;
		else
			reg &= ~I2C3_SCL_GPIO1_3_BIT_MASK;
		writel(reg, GPIO1_BASE_ADDR + GPIO_DR);
		break;
	}
}

/* set 1 to high 0 to low */
static void mx6q_i2c_gpio_sda_set_level(int bus, int high)
{
	u32 reg;

	switch (bus) {
	case 1:
		reg = readl(GPIO5_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C1_SDA_GPIO5_26_BIT_MASK;
		else
			reg &= ~I2C1_SDA_GPIO5_26_BIT_MASK;
		writel(reg, GPIO5_BASE_ADDR + GPIO_DR);
		break;
	case 2:
		reg = readl(GPIO4_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C2_SDA_GPIO4_13_BIT_MASK;
		else
			reg &= ~I2C2_SDA_GPIO4_13_BIT_MASK;
		writel(reg, GPIO4_BASE_ADDR + GPIO_DR);
		break;
	case 3:
		reg = readl(GPIO1_BASE_ADDR + GPIO_DR);
		if (high)
			reg |= I2C3_SDA_GPIO1_6_BIT_MASK;
		else
			reg &= ~I2C3_SDA_GPIO1_6_BIT_MASK;
		writel(reg, GPIO1_BASE_ADDR + GPIO_DR);
		break;
	}
}

static int mx6q_i2c_gpio_check_sda(int bus)
{
	u32 reg;
	int result = 0;

	switch (bus) {
	case 1:
		reg = readl(GPIO5_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C1_SDA_GPIO5_26_BIT_MASK);
		break;
	case 2:
		reg = readl(GPIO4_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C2_SDA_GPIO4_13_BIT_MASK);
		break;
	case 3:
		reg = readl(GPIO1_BASE_ADDR + GPIO_PSR);
		result = !!(reg & I2C3_SDA_GPIO1_6_BIT_MASK);
		break;
	}

	return result;
}

 /* Random reboot cause i2c SDA low issue:
  * the i2c bus busy because some device pull down the I2C SDA
  * line. This happens when Host is reading some byte from slave, and
  * then host is reset/reboot. Since in this case, device is
  * controlling i2c SDA line, the only thing host can do this give the
  * clock on SCL and sending NAK, and STOP to finish this
  * transaction.
  *
  * How to fix this issue:
  * detect if the SDA was low on bus send 8 dummy clock, and 1
  * clock + NAK, and STOP to finish i2c transaction the pending
  * transfer.
  */
int i2c_bus_recovery(void)
{
	int i, bus, result = 0;

	for (bus = 1; bus <= 3; bus++) {
		mx6q_i2c_gpio_sda_direction(bus, 0);

		if (mx6q_i2c_gpio_check_sda(bus) == 0) {
			printf("i2c: I2C%d SDA is low, start i2c recovery...\n", bus);
			mx6q_i2c_gpio_scl_direction(bus, 1);
			mx6q_i2c_gpio_scl_set_level(bus, 1);
			__udelay(10000);

			for (i = 0; i < 9; i++) {
				mx6q_i2c_gpio_scl_set_level(bus, 1);
				__udelay(5);
				mx6q_i2c_gpio_scl_set_level(bus, 0);
				__udelay(5);
			}

			/* 9th clock here, the slave should already
			   release the SDA, we can set SDA as high to
			   a NAK.*/
			mx6q_i2c_gpio_sda_direction(bus, 1);
			mx6q_i2c_gpio_sda_set_level(bus, 1);
			__udelay(1); /* Pull up SDA first */
			mx6q_i2c_gpio_scl_set_level(bus, 1);
			__udelay(5); /* plus pervious 1 us */
			mx6q_i2c_gpio_scl_set_level(bus, 0);
			__udelay(5);
			mx6q_i2c_gpio_sda_set_level(bus, 0);
			__udelay(5);
			mx6q_i2c_gpio_scl_set_level(bus, 1);
			__udelay(5);
			/* Here: SCL is high, and SDA from low to high, it's a
			 * stop condition */
			mx6q_i2c_gpio_sda_set_level(bus, 1);
			__udelay(5);

			mx6q_i2c_gpio_sda_direction(bus, 0);
			if (mx6q_i2c_gpio_check_sda(bus) == 1)
				printf("I2C%d Recovery success\n", bus);
			else {
				printf("I2C%d Recovery failed, I2C1 SDA still low!!!\n", bus);
				result |= 1 << bus;
			}
		}

		/* configure back to i2c */
		switch (bus) {
		case 1:
			setup_i2c(I2C1_BASE_ADDR);
			break;
		case 2:
			setup_i2c(I2C2_BASE_ADDR);
			break;
		case 3:
			setup_i2c(I2C3_BASE_ADDR);
			break;
		}
	}

	return result;
}


void setup_pmic_voltages(void)
{
	unsigned char value = 0 ;
	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
	if (!i2c_probe(0x8)) {
		if (i2c_read(0x8, 0, 1, &value, 1))
			printf("%s:i2c_read:error\n", __func__);
		printf("Found PFUZE100! device id=%x\n", value);
		#if CONFIG_MX6_INTER_LDO_BYPASS
		/*VDDCORE 1.1V@800Mhz: SW1AB*/
		value = 0x20;
		i2c_write(0x8, 0x20, 1, &value, 1);

		/*VDDSOC 1.2V : SW1C*/
		value = 0x24;
		i2c_write(0x8, 0x2e, 1, &value, 1);

		/* Bypass the VDDSOC from Anatop */
		val = REG_RD(ANATOP_BASE_ADDR, HW_ANADIG_REG_CORE);
		val &= ~BM_ANADIG_REG_CORE_REG2_TRG;
		val |= BF_ANADIG_REG_CORE_REG2_TRG(0x1f);
		REG_WR(ANATOP_BASE_ADDR, HW_ANADIG_REG_CORE, val);

		/* Bypass the VDDCORE from Anatop */
		val = REG_RD(ANATOP_BASE_ADDR, HW_ANADIG_REG_CORE);
		val &= ~BM_ANADIG_REG_CORE_REG0_TRG;
		val |= BF_ANADIG_REG_CORE_REG0_TRG(0x1f);
		REG_WR(ANATOP_BASE_ADDR, HW_ANADIG_REG_CORE, val);

		/* Bypass the VDDPU from Anatop */
		val = REG_RD(ANATOP_BASE_ADDR, HW_ANADIG_REG_CORE);
		val &= ~BM_ANADIG_REG_CORE_REG1_TRG;
		val |= BF_ANADIG_REG_CORE_REG1_TRG(0x1f);
		REG_WR(ANATOP_BASE_ADDR, HW_ANADIG_REG_CORE, val);

		/*clear PowerDown Enable bit of WDOG1_WMCR*/
		writew(0, WDOG1_BASE_ADDR + 0x08);
		printf("hw_anadig_reg_core=%x\n",
			REG_RD(ANATOP_BASE_ADDR, HW_ANADIG_REG_CORE));
		#endif

	}
}
#endif

#ifdef CONFIG_IMX_ECSPI
s32 spi_get_cfg(struct imx_spi_dev_t *dev)
{
	switch (dev->slave.cs) {
	case 0:
		/* SPI-NOR */
		dev->base = ECSPI1_BASE_ADDR;
		dev->freq = 25000000;
		dev->ss_pol = IMX_SPI_ACTIVE_HIGH;
		dev->ss = 0;
		dev->fifo_sz = 64 * 4;
		dev->us_delay = 0;
		break;
	case 1:
		/* SPI-NOR */
		dev->base = ECSPI1_BASE_ADDR;
		dev->freq = 25000000;
		dev->ss_pol = IMX_SPI_ACTIVE_LOW;
		dev->ss = 1;
		dev->fifo_sz = 64 * 4;
		dev->us_delay = 0;
		break;
	default:
		printf("Invalid Bus ID!\n");
	}

	return 0;
}

void spi_io_init(struct imx_spi_dev_t *dev)
{
	u32 reg;

	switch (dev->base) {
	case ECSPI1_BASE_ADDR:
#if 0
		/* Enable clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR1);
		reg |= 0xC;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR1);
#endif

#if defined CONFIG_MX6Q
		/* SCLK */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D16__ECSPI1_SCLK);
		//IOMUX_PAD(0x03A4, 0x0090, 1, 0x07F4, 0, 0) | MUX_PAD_CTRL(MX6Q_ECSPI_PAD_CTRL) = 020162 1 7f4 3a4 090

		/* MISO */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D17__ECSPI1_MISO);
		//IOMUX_PAD(0x03A8, 0x0094, 1, 0x07F8, 0, 0) | MUX_PAD_CTRL(MX6Q_ECSPI_PAD_CTRL) = 020162 1 7f8 3a8 094

		/* MOSI */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D18__ECSPI1_MOSI);
		//IOMUX_PAD(0x03AC, 0x0098, 1, 0x07FC, 0, 0) | MUX_PAD_CTRL(MX6Q_ECSPI_PAD_CTRL) = 020162 1 7fc 3ac 098

//		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_RW__ECSPI2_SS0);
		switch (dev->ss)
		{
		case 0:
			mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_EB2__ECSPI1_SS0);
			//IOMUX_PAD(0x03A0, 0x008C, 1, 0x0800, 0, 0) | MUX_PAD_CTRL(MX6Q_ECSPI_PAD_CTRL) = 020162 1 800 3a0 08c
			break;
		case 1:
			mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D19__ECSPI1_SS1);
			//IOMUX_PAD(0x03B0, 0x009C, 1, 0x0804, 0, 0) | MUX_PAD_CTRL(MX6Q_ECSPI_PAD_CTRL) = 020162 1 804 3b0 09c
			break;
		}
#elif defined CONFIG_MX6DL
		/* SCLK */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_OE__ECSPI2_MISO);

		/* MISO */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_OE__ECSPI2_MISO);

		/* MOSI */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_CS1__ECSPI2_MOSI);

		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_RW__ECSPI2_SS0);
#endif
		break;
	case ECSPI2_BASE_ADDR:
	case ECSPI3_BASE_ADDR:
		/* ecspi2,3 fall through */
		break;
	default:
		break;
	}
}
#endif
#ifdef CONFIG_NAND_GPMI
#if defined CONFIG_MX6Q
iomux_v3_cfg_t nfc_pads[] = {
	MX6Q_PAD_NANDF_CLE__RAWNAND_CLE,
	MX6Q_PAD_NANDF_ALE__RAWNAND_ALE,
	MX6Q_PAD_NANDF_WP_B__RAWNAND_RESETN,
	MX6Q_PAD_NANDF_RB0__RAWNAND_READY0,
	MX6Q_PAD_NANDF_CS0__RAWNAND_CE0N,
	MX6Q_PAD_NANDF_CS1__RAWNAND_CE1N,
	MX6Q_PAD_NANDF_CS2__RAWNAND_CE2N,
	MX6Q_PAD_NANDF_CS3__RAWNAND_CE3N,
	MX6Q_PAD_SD4_CMD__RAWNAND_RDN,
	MX6Q_PAD_SD4_CLK__RAWNAND_WRN,
	MX6Q_PAD_NANDF_D0__RAWNAND_D0,
	MX6Q_PAD_NANDF_D1__RAWNAND_D1,
	MX6Q_PAD_NANDF_D2__RAWNAND_D2,
	MX6Q_PAD_NANDF_D3__RAWNAND_D3,
	MX6Q_PAD_NANDF_D4__RAWNAND_D4,
	MX6Q_PAD_NANDF_D5__RAWNAND_D5,
	MX6Q_PAD_NANDF_D6__RAWNAND_D6,
	MX6Q_PAD_NANDF_D7__RAWNAND_D7,
	MX6Q_PAD_SD4_DAT0__RAWNAND_DQS,
};
#elif defined CONFIG_MX6DL
iomux_v3_cfg_t nfc_pads[] = {
	MX6DL_PAD_NANDF_CLE__RAWNAND_CLE,
	MX6DL_PAD_NANDF_ALE__RAWNAND_ALE,
	MX6DL_PAD_NANDF_WP_B__RAWNAND_RESETN,
	MX6DL_PAD_NANDF_RB0__RAWNAND_READY0,
	MX6DL_PAD_NANDF_CS0__RAWNAND_CE0N,
	MX6DL_PAD_NANDF_CS1__RAWNAND_CE1N,
	MX6DL_PAD_NANDF_CS2__RAWNAND_CE2N,
	MX6DL_PAD_NANDF_CS3__RAWNAND_CE3N,
	MX6DL_PAD_SD4_CMD__RAWNAND_RDN,
	MX6DL_PAD_SD4_CLK__RAWNAND_WRN,
	MX6DL_PAD_NANDF_D0__RAWNAND_D0,
	MX6DL_PAD_NANDF_D1__RAWNAND_D1,
	MX6DL_PAD_NANDF_D2__RAWNAND_D2,
	MX6DL_PAD_NANDF_D3__RAWNAND_D3,
	MX6DL_PAD_NANDF_D4__RAWNAND_D4,
	MX6DL_PAD_NANDF_D5__RAWNAND_D5,
	MX6DL_PAD_NANDF_D6__RAWNAND_D6,
	MX6DL_PAD_NANDF_D7__RAWNAND_D7,
	MX6DL_PAD_SD4_DAT0__RAWNAND_DQS,
};
#endif

int setup_gpmi_nand(void)
{
	unsigned int reg;

	/* config gpmi nand iomux */
	mxc_iomux_v3_setup_multiple_pads(nfc_pads,
			ARRAY_SIZE(nfc_pads));


	/* config gpmi and bch clock to 11Mhz*/
	reg = readl(CCM_BASE_ADDR + CLKCTL_CS2CDR);
	reg &= 0xF800FFFF;
	reg |= 0x01E40000;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CS2CDR);

	/* enable gpmi and bch clock gating */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR4);
	reg |= 0xFF003000;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR4);

	/* enable apbh clock gating */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR0);
	reg |= 0x0030;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR0);

}
#endif

#ifdef CONFIG_NET_MULTI
int board_eth_init(bd_t *bis)
{
	int rc = -ENODEV;

	return rc;
}
#endif

#ifdef CONFIG_CMD_MMC

/* On this board, only SD3 can support 1.8V signalling
 * that is required for UHS-I mode of operation.
 * Last element in struct is used to indicate 1.8V support.
 */
struct fsl_esdhc_cfg usdhc_cfg[4] = {
	{USDHC3_BASE_ADDR, 1, 1, 0, 0},
	{USDHC4_BASE_ADDR, 1, 1, 0, 0},
};

#ifdef CONFIG_DYNAMIC_MMC_DEVNO
int get_mmc_env_devno(void)
{
	uint soc_sbmr = readl(SRC_BASE_ADDR + 0x4);

	if (SD_BOOT == boot_dev || MMC_BOOT == boot_dev) {
		/* BOOT_CFG2[3] and BOOT_CFG2[4] */
		return (soc_sbmr & 0x00001800) >> 11;
	} else
		return -1;

}
#endif

#if defined CONFIG_MX6Q
iomux_v3_cfg_t usdhc1_pads[] = {
	MX6Q_PAD_SD3_CLK__USDHC3_CLK, //IOMUX_PAD(0x06a4, 0x02bc, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 6a4 2bc,
	MX6Q_PAD_SD3_CMD__USDHC3_CMD, //IOMUX_PAD(0x06a0, 0x02b8, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e3 0 000 6a0 2b8,
	MX6Q_PAD_SD3_DAT0__USDHC3_DAT0, //IOMUX_PAD(0x06a8, 0x02c0, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 6a8 2c0,
	MX6Q_PAD_SD3_DAT1__USDHC3_DAT1, //IOMUX_PAD(0x06ac, 0x02c4, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 6ac 2c4,
	MX6Q_PAD_SD3_DAT2__USDHC3_DAT2, //IOMUX_PAD(0x06b0, 0x02c8, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 6b0 2c8,
	MX6Q_PAD_SD3_DAT3__USDHC3_DAT3, //IOMUX_PAD(0x06b4, 0x02cc, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 6b4 2cc,
	MX6Q_PAD_SD3_DAT4__USDHC3_DAT4, //IOMUX_PAD(0x069c, 0x02b4, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 69c 2b4,
	MX6Q_PAD_SD3_DAT5__USDHC3_DAT5, //IOMUX_PAD(0x0698, 0x02b0, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 698 2b0,
	MX6Q_PAD_SD3_DAT6__USDHC3_DAT6, //IOMUX_PAD(0x0694, 0x02ac, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 694 2ac,
	MX6Q_PAD_SD3_DAT7__USDHC3_DAT7, //IOMUX_PAD(0x0690, 0x02a8, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 690 2a8,
};

iomux_v3_cfg_t usdhc2_pads[] = {
	MX6Q_PAD_SD4_CLK__USDHC4_CLK, //IOMUX_PAD(0x06e0, 0x02f8, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 0 000 6e0 2f8,
	MX6Q_PAD_SD4_CMD__USDHC4_CMD, //IOMUX_PAD(0x06dc, 0x02f4, 0, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e3 0 000 6dc 2f4,
	MX6Q_PAD_SD4_DAT0__USDHC4_DAT0, //IOMUX_PAD(0x0704, 0x031c, 1, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 1 000 704 31c,
	MX6Q_PAD_SD4_DAT1__USDHC4_DAT1, //IOMUX_PAD(0x0708, 0x0320, 1, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 1 000 708 320,
	MX6Q_PAD_SD4_DAT2__USDHC4_DAT2, //IOMUX_PAD(0x070c, 0x0324, 1, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 1 000 70c 324,
	MX6Q_PAD_SD4_DAT3__USDHC4_DAT3, //IOMUX_PAD(0x0710, 0x0328, 1, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 1 000 710 328,
	MX6Q_PAD_SD4_DAT4__USDHC4_DAT4, //IOMUX_PAD(0x0714, 0x032c, 1, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 1 000 714 32c,
	MX6Q_PAD_SD4_DAT5__USDHC4_DAT5, //IOMUX_PAD(0x0718, 0x0330, 1, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 1 000 718 330,
	MX6Q_PAD_SD4_DAT6__USDHC4_DAT6, //IOMUX_PAD(0x071c, 0x0334, 1, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 1 000 71c 334,
	MX6Q_PAD_SD4_DAT7__USDHC4_DAT7, //IOMUX_PAD(0x0720, 0x0338, 1, 0x0000, 0, 0x170F1) /*| MUX_PAD_CTRL(NO_PAD_CTRL)*/, //0x02e1e2 1 000 720 338,
};
#elif defined CONFIG_MX6DL
iomux_v3_cfg_t usdhc1_pads[] = {
	MX6DL_PAD_SD1_CLK__USDHC1_CLK,
	MX6DL_PAD_SD1_CMD__USDHC1_CMD,
	MX6DL_PAD_SD1_DAT0__USDHC1_DAT0,
	MX6DL_PAD_SD1_DAT1__USDHC1_DAT1,
	MX6DL_PAD_SD1_DAT2__USDHC1_DAT2,
	MX6DL_PAD_SD1_DAT3__USDHC1_DAT3,
};

iomux_v3_cfg_t usdhc2_pads[] = {
	MX6DL_PAD_SD2_CLK__USDHC2_CLK,
	MX6DL_PAD_SD2_CMD__USDHC2_CMD,
	MX6DL_PAD_SD2_DAT0__USDHC2_DAT0,
	MX6DL_PAD_SD2_DAT1__USDHC2_DAT1,
	MX6DL_PAD_SD2_DAT2__USDHC2_DAT2,
	MX6DL_PAD_SD2_DAT3__USDHC2_DAT3,
};
#endif

int usdhc_gpio_init(bd_t *bis)
{
	s32 status = 0;
	u32 index = 0;

	for (index = 0; index < CONFIG_SYS_FSL_USDHC_NUM;
		++index) {
		switch (index) {
		case 0:
			mxc_iomux_v3_setup_multiple_pads(usdhc1_pads,
				sizeof(usdhc1_pads) /
				sizeof(usdhc1_pads[0]));
			break;
		case 1:
			mxc_iomux_v3_setup_multiple_pads(usdhc2_pads,
				sizeof(usdhc2_pads) /
				sizeof(usdhc2_pads[0]));
			break;
		}
		status |= fsl_esdhc_initialize(bis, &usdhc_cfg[index]);
	}

	return status;
}

int board_mmc_init(bd_t *bis)
{
	if (!usdhc_gpio_init(bis))
		return 0;
	else
		return -1;
}

/* For DDR mode operation, provide target delay parameter for each SD port.
 * Use cfg->esdhc_base to distinguish the SD port #. The delay for each port
 * is dependent on signal layout for that particular port.  If the following
 * CONFIG is not defined, then the default target delay value will be used.
 */
#ifdef CONFIG_GET_DDR_TARGET_DELAY
u32 get_ddr_delay(struct fsl_esdhc_cfg *cfg)
{
	/* No delay required on SABRESD board SD ports */
	return 0;
}
#endif

#endif

#ifdef CONFIG_LCD
void lcd_enable(void)
{
	char *s;
	int ret;
	unsigned int reg;

	s = getenv("lvds_num");
	di = simple_strtol(s, NULL, 10);

	/*
	* hw_rev 2: IPUV3DEX
	* hw_rev 3: IPUV3M
	* hw_rev 4: IPUV3H
	*/
	g_ipu_hw_rev = IPUV3_HW_REV_IPUV3H;

#if defined CONFIG_MX6Q
	mxc_iomux_v3_setup_pad(MX6Q_PAD_SD1_DAT3__GPIO_1_21);
#elif defined CONFIG_MX6DL
	mxc_iomux_v3_setup_pad(MX6DL_PAD_SD1_DAT3__GPIO_1_21);
#endif

	reg = readl(GPIO1_BASE_ADDR + GPIO_GDIR);
	reg |= (1 << 21);
	writel(reg, GPIO1_BASE_ADDR + GPIO_GDIR);

	reg = readl(GPIO1_BASE_ADDR + GPIO_DR);
	reg |= (1 << 21);
	writel(reg, GPIO1_BASE_ADDR + GPIO_DR);

	/* Enable IPU clock */
	if (di == 1) {
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR3);
		reg |= 0xC033;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR3);
	} else {
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR3);
		reg |= 0x300F;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR3);
	}

	ret = ipuv3_fb_init(&lvds_xga, di, IPU_PIX_FMT_RGB666,
			DI_PCLK_LDB, 65000000);
	if (ret)
		puts("LCD cannot be configured\n");

	reg = readl(ANATOP_BASE_ADDR + 0xF0);
	reg &= ~0x00003F00;
	reg |= 0x00001300;
	writel(reg, ANATOP_BASE_ADDR + 0xF4);

	reg = readl(CCM_BASE_ADDR + CLKCTL_CS2CDR);
	reg &= ~0x00007E00;
	reg |= 0x00003600;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CS2CDR);

	reg = readl(CCM_BASE_ADDR + CLKCTL_CSCMR2);
	reg |= 0x00000C00;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CSCMR2);

	reg = 0x0002A953;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CHSCCDR);

	if (di == 1)
		writel(0x40C, IOMUXC_BASE_ADDR + 0x8);
	else
		writel(0x201, IOMUXC_BASE_ADDR + 0x8);
}
#endif

#ifdef CONFIG_VIDEO_MX5
void panel_info_init(void)
{
	panel_info.vl_bpix = LCD_BPP;
	panel_info.vl_col = lvds_xga.xres;
	panel_info.vl_row = lvds_xga.yres;
	panel_info.cmap = colormap;
}
#endif

#ifdef CONFIG_SPLASH_SCREEN
void setup_splash_image(void)
{
	char *s;
	ulong addr;

	s = getenv("splashimage");

	if (s != NULL) {
		addr = simple_strtoul(s, NULL, 16);

#if defined(CONFIG_ARCH_MMU)
		addr = ioremap_nocache(iomem_to_phys(addr),
				fsl_bmp_600x400_size);
#endif
		memcpy((char *)addr, (char *)fsl_bmp_600x400,
				fsl_bmp_600x400_size);
	}
}
#endif

int board_init(void)
{
/* need set Power Supply Glitch to 0x41736166
*and need clear Power supply Glitch Detect bit
* when POR or reboot or power on Otherwise system
*could not be power off anymore*/
	u32 reg;
	writel(0x41736166, SNVS_BASE_ADDR + 0x64);/*set LPPGDR*/
	udelay(10);
	reg = readl(SNVS_BASE_ADDR + 0x4c);
	reg |= (1 << 3);
	writel(reg, SNVS_BASE_ADDR + 0x4c);/*clear LPSR*/

	mxc_iomux_v3_init((void *)IOMUXC_BASE_ADDR);
#if 0
#if defined CONFIG_MX6Q
	mxc_iomux_v3_setup_pad(MX6Q_PAD_NANDF_D6__GPIO_2_6);
#elif defined CONFIG_MX6DL
	mxc_iomux_v3_setup_pad(MX6DL_PAD_NANDF_D6__GPIO_2_6);
#endif
	gpio_direction_output(HDMIDONGLE_CHG_SYS_ON, 1);
#else
	func_27800f08();
#endif
	setup_boot_device();
	fsl_set_system_rev();

	/* board id for linux */
	gd->bd->bi_arch_number = MACH_TYPE_MX6Q_RICHTECHIE;

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	setup_uart();

	mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_5__I2C3_SCL);
	//IOMUX_PAD(0x060C, 0x023C, 6 | IOMUX_CONFIG_SION, 0x08A8, 2, 0) | MUX_PAD_CTRL(MX6Q_I2C_PAD_CTRL) = 237163 6 8a8 60c 23c
	mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_16__I2C3_SDA);
	//IOMUX_PAD(0x0618, 0x0248, 6 | IOMUX_CONFIG_SION, 0x08AC, 2, 0) | MUX_PAD_CTRL(MX6Q_I2C_PAD_CTRL) = 237163 6 8ac 618 248

	reg = readl(CCM_BASE_ADDR + 0x70);
	reg |= 0xc00;
	writel(reg, CCM_BASE_ADDR + 0x70);

#ifdef CONFIG_VIDEO_MX5
	/* Enable lvds power */
	setup_lvds_poweron();

	panel_info_init();

	gd->fb_base = CONFIG_FB_BASE;
#ifdef CONFIG_ARCH_MMU
	gd->fb_base = ioremap_nocache(iomem_to_phys(gd->fb_base), 0);
#endif
#endif

#ifdef CONFIG_NAND_GPMI
	setup_gpmi_nand();
#endif
	return 0;
}


#ifdef CONFIG_ANDROID_RECOVERY
int check_recovery_cmd_file(void)
{
	disk_partition_t info;
	ulong part_length;
	int filelen = 0;
	int dev = 1;
	char *env;

	/* For test only */
	/* When detecting android_recovery_switch,
	 * enter recovery mode directly */
	env = getenv("android_recovery_switch");
	if (!strcmp(env, "1")) {
		printf("Env recovery detected!\nEnter recovery mode!\n");
		return 1;
	}

	printf("Checking for recovery command file...\n");
	switch (get_boot_device()) {
	case I2C_BOOT:
	case SPI_NOR_BOOT:
	case MMC_BOOT:
	case SD_BOOT:
		{
			block_dev_desc_t *dev_desc = NULL;
			struct mmc *mmc = find_mmc_device(dev);

			dev_desc = get_dev("mmc", dev);

			if (NULL == dev_desc) {
				printf("** Block device MMC %d not supported\n", dev-1);
				break;
			}

			mmc_init(mmc);

			if (get_partition_info(dev_desc,
					CONFIG_ANDROID_CACHE_PARTITION_MMC,
					&info)) {
				printf("** Bad partition %d **\n",
					CONFIG_ANDROID_CACHE_PARTITION_MMC);
				break;
			}

			part_length = ext2fs_set_blk_dev(dev_desc,
					CONFIG_ANDROID_CACHE_PARTITION_MMC);
			if (part_length == 0) {
				printf("** Bad partition - mmc %d:%d **\n", dev-1,
					CONFIG_ANDROID_CACHE_PARTITION_MMC);
				ext2fs_close();
				break;
			}

			if (!ext2fs_mount(part_length)) {
				printf("** Bad ext2 partition or "
					"disk - mmc %d:%d **\n", dev-1,
					CONFIG_ANDROID_CACHE_PARTITION_MMC);
				ext2fs_close();
				break;
			}

			filelen = ext2fs_open(CONFIG_ANDROID_RECOVERY_CMD_FILE);

			ext2fs_close();
		}
		break;
	default:
		return 0;
		break;
	}

	return (filelen > 0) ? 1 : 0;
}
#endif

int board_late_init(void)
{
	return 0;
}

#ifdef CONFIG_MXC_FEC
static int phy_read(char *devname, unsigned char addr, unsigned char reg,
		    unsigned short *pdata)
{
	int ret = miiphy_read(devname, addr, reg, pdata);
	if (ret)
		printf("Error reading from %s PHY addr=%02x reg=%02x\n",
		       devname, addr, reg);
	return ret;
}

static int phy_write(char *devname, unsigned char addr, unsigned char reg,
		     unsigned short value)
{
	int ret = miiphy_write(devname, addr, reg, value);
	if (ret)
		printf("Error writing to %s PHY addr=%02x reg=%02x\n", devname,
		       addr, reg);
	return ret;
}

int mx6_rgmii_rework(char *devname, int phy_addr)
{
	phy_write(devname, phy_addr, 0x9, 0x1c00);
	phy_write(devname, phy_addr, 0xb, 0x8105);
	phy_write(devname, phy_addr, 0xc, 0x00);
	phy_write(devname, phy_addr, 0xb, 0x8104);
	phy_write(devname, phy_addr, 0xc, 0xf0f0);
	phy_write(devname, phy_addr, 0xb, 0x0104);

	return 0;
}

#if defined CONFIG_MX6Q
iomux_v3_cfg_t enet_pads[] = {
	MX6Q_PAD_ENET_MDIO__ENET_MDIO,
	MX6Q_PAD_ENET_MDC__ENET_MDC,
	MX6Q_PAD_RGMII_TXC__ENET_RGMII_TXC,
	MX6Q_PAD_RGMII_TD0__ENET_RGMII_TD0,
	MX6Q_PAD_RGMII_TD1__ENET_RGMII_TD1,
	MX6Q_PAD_RGMII_TD2__ENET_RGMII_TD2,
	MX6Q_PAD_RGMII_TD3__ENET_RGMII_TD3,
	MX6Q_PAD_RGMII_TX_CTL__ENET_RGMII_TX_CTL,
	MX6Q_PAD_ENET_REF_CLK__ENET_TX_CLK,
	MX6Q_PAD_RGMII_RXC__ENET_RGMII_RXC,
	MX6Q_PAD_RGMII_RD0__ENET_RGMII_RD0,
	MX6Q_PAD_RGMII_RD1__ENET_RGMII_RD1,
	MX6Q_PAD_RGMII_RD2__ENET_RGMII_RD2,
	MX6Q_PAD_RGMII_RD3__ENET_RGMII_RD3,
	MX6Q_PAD_RGMII_RX_CTL__ENET_RGMII_RX_CTL,
	MX6Q_PAD_GPIO_0__CCM_CLKO,
	MX6Q_PAD_GPIO_3__CCM_CLKO2,
};
#elif defined CONFIG_MX6DL
iomux_v3_cfg_t enet_pads[] = {
	MX6DL_PAD_ENET_MDIO__ENET_MDIO,
	MX6DL_PAD_ENET_MDC__ENET_MDC,
	MX6DL_PAD_RGMII_TXC__ENET_RGMII_TXC,
	MX6DL_PAD_RGMII_TD0__ENET_RGMII_TD0,
	MX6DL_PAD_RGMII_TD1__ENET_RGMII_TD1,
	MX6DL_PAD_RGMII_TD2__ENET_RGMII_TD2,
	MX6DL_PAD_RGMII_TD3__ENET_RGMII_TD3,
	MX6DL_PAD_RGMII_TX_CTL__ENET_RGMII_TX_CTL,
	MX6DL_PAD_ENET_REF_CLK__ENET_TX_CLK,
	MX6DL_PAD_RGMII_RXC__ENET_RGMII_RXC,
	MX6DL_PAD_RGMII_RD0__ENET_RGMII_RD0,
	MX6DL_PAD_RGMII_RD1__ENET_RGMII_RD1,
	MX6DL_PAD_RGMII_RD2__ENET_RGMII_RD2,
	MX6DL_PAD_RGMII_RD3__ENET_RGMII_RD3,
	MX6DL_PAD_RGMII_RX_CTL__ENET_RGMII_RX_CTL,
	MX6DL_PAD_GPIO_0__CCM_CLKO,
};
#endif

void enet_board_init(void)
{
}
#endif

int checkboard(void)
{
	printf("Board: %s-HDMIDONGLE: %s Board: 0x%x [",
	mx6_chip_name(),
	mx6_board_rev_name(),
	fsl_system_rev);

	switch (__REG(SRC_BASE_ADDR + 0x8)) {
	case 0x0001:
		printf("POR");
		break;
	case 0x0009:
		printf("RST");
		break;
	case 0x0010:
	case 0x0011:
		printf("WDOG");
		break;
	default:
		printf("unknown");
	}
	printf(" ]\n");

	printf("Boot Device: ");
	switch (get_boot_device()) {
	case WEIM_NOR_BOOT:
		printf("NOR\n");
		break;
	case ONE_NAND_BOOT:
		printf("ONE NAND\n");
		break;
	case PATA_BOOT:
		printf("PATA\n");
		break;
	case SATA_BOOT:
		printf("SATA\n");
		break;
	case I2C_BOOT:
		printf("I2C\n");
		break;
	case SPI_NOR_BOOT:
		printf("SPI NOR\n");
		break;
	case SD_BOOT:
		printf("SD\n");
		break;
	case MMC_BOOT:
		printf("MMC\n");
		break;
	case NAND_BOOT:
		printf("NAND\n");
		break;
	case UNKNOWN_BOOT:
	default:
		printf("UNKNOWN\n");
		break;
	}
	return 0;
}

#ifdef CONFIG_IMX_UDC

void udc_pins_setting(void)
{
	int reg;

	mxc_iomux_v3_setup_pad(MX6X_IOMUX(PAD_GPIO_1__USBOTG_ID));
	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D31__GPIO_3_31);
	//IOMUX_PAD(0x03e4, 0x00d0, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 3e4 0d0

	reg = readl(GPIO3_BASE_ADDR+4);
	reg |= (1 << 31);
	writel(reg, GPIO3_BASE_ADDR+4);

	reg = readl(GPIO3_BASE_ADDR);
	reg |= (1 << 31);
	writel(reg, GPIO3_BASE_ADDR);

#if 0
	mxc_iomux_v3_setup_pad(MX6X_IOMUX(PAD_KEY_ROW4__GPIO_4_15));
	if (mx6_board_is_reva())
		mxc_iomux_v3_setup_pad(MX6X_IOMUX(PAD_KEY_COL4__GPIO_4_14));

	/* set USB_OTG_PWR to 0 */
	gpio_direction_output(USB_OTG_PWR, 0);

	/* set USB_H1_POWER to 1 */
	if (mx6_board_is_reva())
		gpio_direction_output(USB_H1_POWER, 1);
#endif

	mxc_iomux_set_gpr_register(1, 13, 1, 1);

}
#endif


void func_2780169c()
{
	int reg;

	reg = readl(GPIO2_BASE_ADDR);
	reg &= ~(1 << 5);
	writel(reg, GPIO2_BASE_ADDR);

	reg = readl(GPIO1_BASE_ADDR);
	reg &= ~(1 << 8);
	writel(reg, GPIO1_BASE_ADDR);

	imx_pwm_disable(pwm);
}


void func_27800ebc(int a)
{
	int reg;

	mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_18__GPIO_7_13);
	//IOMUX_PAD(0x0620, 0x0250, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 620 250

	reg = readl(GPIO7_BASE_ADDR+4);
	reg |= (1 << 13);
	writel(reg, GPIO7_BASE_ADDR+4);

	reg = readl(GPIO7_BASE_ADDR);
	if (a)
		reg |= (1 << 13);
	else
		reg &= ~(1 << 13);
	writel(reg, GPIO7_BASE_ADDR);
}


int func_27800de8()
{
	int reg;

	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D16__GPIO_3_16);
	//IOMUX_PAD(0x03a4, 0x0090, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 3a4 090

	reg = readl(GPIO3_BASE_ADDR+4);
	reg &= ~(1 << 16);
	writel(reg, GPIO3_BASE_ADDR+4);

	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D20__GPIO_3_20);
	//IOMUX_PAD(0x03b4, 0x00a0, 5, 0x0000, 0, 0) | MUX_PAD_CTRL(NO_PAD_CTRL) = 040000 5 000 3b4 0a0

	reg = readl(GPIO3_BASE_ADDR+4);
	reg &= ~(1 << 20);
	writel(reg, GPIO3_BASE_ADDR+4);

	return 0;
}


int func_27800da4()
{
	int reg;
	int res;

	reg = readl(GPIO3_BASE_ADDR);

	if ((reg & 0x110000) == 0x110000)
		return 0;

	reg = readl(GPIO3_BASE_ADDR);

	if (reg & 0x10000)
		res = 0;
	else
		res = 2;

	if (reg & 0x100000)
		return res;

	if (res)
		return 6;
	else
		return 4;
}

