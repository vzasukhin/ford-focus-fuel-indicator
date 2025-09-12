#include <stdint.h>
#include <string.h>

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_conf.h"

#include "common.h"
#include "flash.h"
#include "gpio.h"
#include "rcc.h"
#include "usart.h"


#define FIRMWARE_ADDR	0x08002000

#define HELLO_MESSAGE 0xaa035539
#define RESP_ACK 0x01
#define RESP_NACK 0xa5
#define CMD_FLASH 0x1
#define CMD_RESET 0x2
#define CMD_LOAD 0x3

#define GPIO_LED_SMALL	GEN_GPIO(BANK_GPIOC, 13)
#define GPIO_STEP	GEN_GPIO(BANK_GPIOB, 0)
#define GPIO_DIR	GEN_GPIO(BANK_GPIOB, 1)

#define UART_NUM 1

struct packet_hdr {
	uint8_t cmd;
	uint8_t reserved;
	uint16_t len;
	uint16_t offset;
	uint16_t crc;
} __attribute__((packed));

static void Error_Handler(void)
{
	while (1) {
	}
}

void SysTick_Handler(void)
{
	HAL_IncTick();
}

static void SystemClock_Config(void)
{
	RCC_ClkInitTypeDef clkinitstruct = {
		.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 |
			     RCC_CLOCKTYPE_PCLK2,
		.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK,
		.AHBCLKDivider = RCC_SYSCLK_DIV1,
		.APB1CLKDivider = RCC_HCLK_DIV2,
		.APB2CLKDivider = RCC_HCLK_DIV1,
	};
	RCC_OscInitTypeDef oscinitstruct = {
		.OscillatorType  = RCC_OSCILLATORTYPE_HSE,  // Expected 8 MHz resonator
		.HSEState        = RCC_HSE_ON,
		.HSEPredivValue  = RCC_HSE_PREDIV_DIV1,
		.PLL.PLLMUL      = RCC_PLL_MUL9,  // SysClk 72 MHz
		.PLL.PLLState    = RCC_PLL_ON,
		.PLL.PLLSource   = RCC_PLLSOURCE_HSE,
	};
	RCC_PeriphCLKInitTypeDef rccperiphclkinit = {
		.PeriphClockSelection = RCC_PERIPHCLK_ADC,
		.RTCClockSelection = RCC_RTCCLKSOURCE_LSI,
		.AdcClockSelection = RCC_ADCPCLK2_DIV6,
	};

	// Enable HSE Oscillator and activate PLL with HSE as source
	if (HAL_RCC_OscConfig(&oscinitstruct)!= HAL_OK)
		Error_Handler();

	// USB clock selection
	HAL_RCCEx_PeriphCLKConfig(&rccperiphclkinit);

	// Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers
//	if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2)!= HAL_OK)
	if (HAL_RCC_ClockConfig(&clkinitstruct, 2)!= HAL_OK)
		Error_Handler();
}

bool wait_for_packet(void)
{
	uint32_t hello_msg = 0;
	uint8_t buf[1024];
	struct packet_hdr hdr;
	uint8_t *hdr_ptr = (uint8_t *)&hdr;
	uint16_t crc = 0xffff;

	while (hello_msg != HELLO_MESSAGE)
		hello_msg = (hello_msg << 8) | usart_wait_and_recv_byte(UART_NUM);

	usart_send_byte(UART_NUM, RESP_ACK);
	for (int i = 0; i < 8; i++)
		hdr_ptr[i] = usart_wait_and_recv_byte(UART_NUM);

	if (hdr.cmd == CMD_FLASH) {
		if (hdr.len > 1024 || (hdr.offset + hdr.len) > 55*1024)  // 55 KiB is with reserved last page
			usart_send_byte(UART_NUM, RESP_NACK);
		else
			usart_send_byte(UART_NUM, RESP_ACK);

		for (int i = 0; i < hdr.len; i++) {
			buf[i] = usart_wait_and_recv_byte(UART_NUM);
			crc ^= (uint16_t)buf[i] << 8;
			for (int j = 0; j < 8; j++)
				crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
		}

		if (crc != hdr.crc) {
			usart_send_byte(UART_NUM, RESP_NACK);
			return false;
		}

		if (flash_erase_page(FIRMWARE_ADDR + hdr.offset)) {
			usart_send_byte(UART_NUM, RESP_NACK);
			return false;
		}

		if (flash_program(FIRMWARE_ADDR + hdr.offset, buf, hdr.len)) {
			usart_send_byte(UART_NUM, RESP_NACK);
			return false;
		}

		if (flash_verify(FIRMWARE_ADDR + hdr.offset, buf, hdr.len)) {
			usart_send_byte(UART_NUM, RESP_NACK);
			return false;
		}

		usart_send_byte(UART_NUM, RESP_ACK);
		return false;
	} else if (hdr.cmd == CMD_RESET) {
		uint32_t value;
		usart_send_byte(UART_NUM, RESP_ACK);
		usart_flush(UART_NUM);
		value = readl(0xe000ed0c);  // SCB_AIRCR register
		value = (value & 0xffff) | (0x5fa << 16);  // VECTKEY required to write to this register
		value |= BIT(2);  // SYSRESETREQ
		writel(value, 0xe000ed0c);
		return false;
	} else if (hdr.cmd == CMD_LOAD) {
		usart_send_byte(UART_NUM, RESP_ACK);
		return true;
	} else {
		usart_send_byte(UART_NUM, RESP_NACK);
		return false;
	}
}

void receive_firmware(void)
{
	usart_send_byte(UART_NUM, RESP_ACK);
	while (!wait_for_packet()) {
	}
}

int main(void)
{
	uint32_t tick_start;
	int count = 3;
	uint32_t hello_msg = 0;

	SystemClock_Config();
	SystemCoreClockUpdate();
	HAL_Init();

	rcc_clk_enable(RCC_CLK_GPIOA);
	rcc_clk_enable(RCC_CLK_USART1);

	gpio_init(GEN_GPIO(BANK_GPIOA, 9), GPIO_DIR_OUT, GPIO_DRV_PP, GPIO_SPEED_MEDIUM, GPIO_FLAG_ALTERNATE);
	gpio_init(GEN_GPIO(BANK_GPIOA, 10), GPIO_DIR_IN, GPIO_DRV_PP, GPIO_SPEED_MEDIUM, GPIO_FLAG_ALTERNATE);

	usart_init(UART_NUM, 72000000, 115200);

	HAL_Delay(200);

	usart_puts(UART_NUM, "Ready");
	tick_start = HAL_GetTick();
	while (count) {
		if (usart_is_received(UART_NUM)) {
			hello_msg = (hello_msg << 8) | usart_recv_byte(UART_NUM);
			if (hello_msg == HELLO_MESSAGE) {
				receive_firmware();
				break;
			}
		}

		if (HAL_GetTick() - tick_start >= 1000) {
			tick_start = HAL_GetTick();
			count--;
			usart_puts(UART_NUM, " Ready");
		}
	}

	usart_puts(UART_NUM, "\nStarting main firmware...\n");
	usart_flush(UART_NUM);
	__asm volatile ("b %0" :: "i"(FIRMWARE_ADDR) : "memory");
}
