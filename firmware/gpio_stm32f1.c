#include "common.h"
#include "gpio.h"

#define GPIO_REG_CRL	0
#define GPIO_REG_CRH	0x4
#define GPIO_REG_IDR	0x8
#define GPIO_REG_ODR	0xc
#define GPIO_REG_BSRR	0x10
#define GPIO_REG_BRR	0x14
#define GPIO_REG_LCKR	0x18

#define GPIO_BANK_TO_ADDR(b) (0x40010800 + ((b) * 0x400))

void gpio_init(gpio_t gpio, uint8_t dir, uint8_t drv, uint8_t speed, uint32_t flags)
{
	uintptr_t addr = GPIO_BANK_TO_ADDR(GPIO_TO_BANK(gpio));
	uint8_t pin = GPIO_TO_PIN(gpio);
	uint32_t val = 0;
	uint32_t reg_val;

	if (dir == GPIO_DIR_IN) {
		if (!(flags & GPIO_FLAG_ANALOG)) {
			if (flags & (GPIO_FLAG_PU | GPIO_FLAG_PD)) {
				val = 0b1000;
				if (flags & GPIO_FLAG_PU)
					writel(BIT(pin), addr + GPIO_REG_BSRR);
				else
					writel(BIT(pin), addr + GPIO_REG_BRR);
			} else
				val = 0b0100;
		}
	} else {
		val = (flags & GPIO_FLAG_ALTERNATE) | speed | drv;
	}
	if (pin >> 3) {
		pin &= 0x7;
		addr += GPIO_REG_CRH;
	}

	reg_val = readl(addr);
	reg_val = (reg_val & ~(0xf << (pin * 4))) | (val << (pin * 4));
	writel(reg_val, addr);
}

void gpio_pin_set(gpio_t gpio, uint32_t val)
{
	uintptr_t addr = GPIO_BANK_TO_ADDR(GPIO_TO_BANK(gpio));
	uint8_t pin = GPIO_TO_PIN(gpio);

	if (val)
		writel(BIT(pin), addr + GPIO_REG_BSRR);
	else
		writel(BIT(pin), addr + GPIO_REG_BRR);
}

uint32_t gpio_pin_get(gpio_t gpio)
{
	uintptr_t addr = GPIO_BANK_TO_ADDR(GPIO_TO_BANK(gpio));
	uint8_t pin = GPIO_TO_PIN(gpio);

	return (readl(addr + GPIO_REG_IDR) >> pin) & 0x1;
}

void gpio_pin_toggle(gpio_t gpio)
{
	gpio_pin_set(gpio, !gpio_pin_get(gpio));
}

uint16_t gpio_bank_get(uint8_t bank)
{
	uintptr_t addr = GPIO_BANK_TO_ADDR(bank);

	return readl(addr + GPIO_REG_IDR);
}
