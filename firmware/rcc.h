#ifndef _RCC_H
#define _RCC_H

#include <stdint.h>

#include <common.h>

#ifdef STM32F1
#define RCC_BASE_ADDR 0x40021000
#define RCC_AHBENR (RCC_BASE_ADDR + 0x14)
#define RCC_APB2ENR (RCC_BASE_ADDR + 0x18)
#define RCC_APB1ENR (RCC_BASE_ADDR + 0x1c)

#define RCC_CLK_SDIO	RCC_AHBENR, BIT(10)
#define RCC_CLK_FSMC	RCC_AHBENR, BIT(8)
#define RCC_CLK_CRC	RCC_AHBENR, BIT(6)
#define RCC_CLK_FLITF	RCC_AHBENR, BIT(4)
#define RCC_CLK_SRAM	RCC_AHBENR, BIT(2)
#define RCC_CLK_DMA2	RCC_AHBENR, BIT(1)
#define RCC_CLK_DMA1	RCC_AHBENR, BIT(0)

#define RCC_CLK_TIM11	RCC_APB2ENR, BIT(21)
#define RCC_CLK_TIM10	RCC_APB2ENR, BIT(20)
#define RCC_CLK_TIM9	RCC_APB2ENR, BIT(19)
#define RCC_CLK_ADC3	RCC_APB2ENR, BIT(15)
#define RCC_CLK_USART1	RCC_APB2ENR, BIT(14)
#define RCC_CLK_TIM8	RCC_APB2ENR, BIT(13)
#define RCC_CLK_SPI1	RCC_APB2ENR, BIT(12)
#define RCC_CLK_TIM1	RCC_APB2ENR, BIT(11)
#define RCC_CLK_ADC2	RCC_APB2ENR, BIT(10)
#define RCC_CLK_ADC1	RCC_APB2ENR, BIT(9)
#define RCC_CLK_GPIOG	RCC_APB2ENR, BIT(8)
#define RCC_CLK_GPIOF	RCC_APB2ENR, BIT(7)
#define RCC_CLK_GPIOE	RCC_APB2ENR, BIT(6)
#define RCC_CLK_GPIOD	RCC_APB2ENR, BIT(5)
#define RCC_CLK_GPIOC	RCC_APB2ENR, BIT(4)
#define RCC_CLK_GPIOB	RCC_APB2ENR, BIT(3)
#define RCC_CLK_GPIOA	RCC_APB2ENR, BIT(2)
#define RCC_CLK_AFIO	RCC_APB2ENR, BIT(0)

#define RCC_CLK_DAC	RCC_APB1ENR, BIT(29)
#define RCC_CLK_PWR	RCC_APB1ENR, BIT(28)
#define RCC_CLK_BKP	RCC_APB1ENR, BIT(27)
#define RCC_CLK_CAN	RCC_APB1ENR, BIT(25)
#define RCC_CLK_USB	RCC_APB1ENR, BIT(23)
#define RCC_CLK_I2C2	RCC_APB1ENR, BIT(22)
#define RCC_CLK_I2C1	RCC_APB1ENR, BIT(21)
#define RCC_CLK_UART5	RCC_APB1ENR, BIT(20)
#define RCC_CLK_UART4	RCC_APB1ENR, BIT(19)
#define RCC_CLK_USART3	RCC_APB1ENR, BIT(18)
#define RCC_CLK_USART2	RCC_APB1ENR, BIT(17)
#define RCC_CLK_SPI3	RCC_APB1ENR, BIT(15)
#define RCC_CLK_SPI2	RCC_APB1ENR, BIT(14)
#define RCC_CLK_WWDG	RCC_APB1ENR, BIT(11)
#define RCC_CLK_TIM14	RCC_APB1ENR, BIT(8)
#define RCC_CLK_TIM13	RCC_APB1ENR, BIT(7)
#define RCC_CLK_TIM12	RCC_APB1ENR, BIT(6)
#define RCC_CLK_TIM7	RCC_APB1ENR, BIT(5)
#define RCC_CLK_TIM6	RCC_APB1ENR, BIT(4)
#define RCC_CLK_TIM5	RCC_APB1ENR, BIT(3)
#define RCC_CLK_TIM4	RCC_APB1ENR, BIT(2)
#define RCC_CLK_TIM3	RCC_APB1ENR, BIT(1)
#define RCC_CLK_TIM2	RCC_APB1ENR, BIT(0)
#endif

inline static void rcc_clk_enable(uintptr_t reg_addr, uint32_t mask)
{
	uint32_t val = readl(reg_addr);

	writel(val | mask, reg_addr);
	(void)readl(reg_addr);  // delay as in driver from vendor
}

inline static void rcc_clk_disable(uintptr_t reg_addr, uint32_t mask)
{
	uint32_t val = readl(reg_addr);

	writel(val | mask, reg_addr);
	(void)readl(reg_addr);  // delay as in driver from vendor
}

#endif  // _RCC_H
