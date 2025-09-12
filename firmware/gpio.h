#ifndef _GPIO_H
#define _GPIO_H

#include <stdint.h>

#ifdef STM32F1
#define GPIO_DIR_IN 0
#define GPIO_DIR_OUT 1
#define GPIO_DRV_PP 0b0000  // hardware depended value
#define GPIO_DRV_OD 0b0100  // hardware depended value
#define GPIO_SPEED_LOW 0x2  // hardware depended value
#define GPIO_SPEED_MEDIUM 0x1  // hardware depended value
#define GPIO_SPEED_HIGH 0x3  // hardware depended value
#define GPIO_SPEED_VERYHIGH 0x3  // hardware depended value
#define GPIO_FLAG_PU 0x1
#define GPIO_FLAG_PD 0x2
#define GPIO_FLAG_ANALOG 0x4
#define GPIO_FLAG_ALTERNATE 0x8  // hardware depended value
#endif  // STM32F1

#define GEN_GPIO(bank, pin) (((bank) << 4) | (pin))
#define GPIO_TO_BANK(gpio) ((gpio) >> 4)
#define GPIO_TO_PIN(gpio) ((gpio) & 0xf)

#define BANK_GPIOA 0
#define BANK_GPIOB 1
#define BANK_GPIOC 2
#define BANK_GPIOD 3
#define BANK_GPIOE 4
#define BANK_GPIOF 5
#define BANK_GPIOG 6

typedef uint8_t gpio_t;

void gpio_init(gpio_t gpio, uint8_t dir, uint8_t drv, uint8_t speed, uint32_t flags);
void gpio_pin_set(gpio_t gpio, uint32_t val);
uint32_t gpio_pin_get(gpio_t gpio);
void gpio_pin_toggle(gpio_t gpio);
uint16_t gpio_bank_get(uint8_t bank);

#endif  // _GPIO_H
