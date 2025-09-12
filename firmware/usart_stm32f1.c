#include <stdarg.h>
#include <stdbool.h>

#include "common.h"
#include "usart.h"

#define SR_TXE BIT(7)
#define SR_TC BIT(6)
#define SR_RXNE BIT(5)
#define SR_IDLE BIT(4)

#define CR1_UE BIT(13)
#define CR1_TE BIT(3)
#define CR1_RE BIT(2)

#define USART1_BASE_ADDR 0x40013800
#define USART2_BASE_ADDR 0x40004400
#define USART3_BASE_ADDR 0x40004800
#define UART4_BASE_ADDR 0x40004c00
#define UART5_BASE_ADDR 0x40005000

typedef struct {
	volatile uint32_t SR;
	volatile uint32_t DR;
	volatile uint32_t BRR;
	volatile uint32_t CR1;
	volatile uint32_t CR2;
	volatile uint32_t CR3;
	volatile uint32_t GTPR;
} usart_regs_t;

static usart_regs_t *get_usart_regs(uint8_t num)
{
	if (num == 1)
		return (usart_regs_t *)USART1_BASE_ADDR;
	else if (num == 2)
		return (usart_regs_t *)USART2_BASE_ADDR;
	else if (num == 3)
		return (usart_regs_t *)USART3_BASE_ADDR;
	else if (num == 4)
		return (usart_regs_t *)UART4_BASE_ADDR;
	else if (num == 5)
		return (usart_regs_t *)UART5_BASE_ADDR;
	else
		return NULL;
}

void usart_init(uint8_t num, uint32_t pclk, uint32_t boudrate)
{
	usart_regs_t *regs = get_usart_regs(num);
	uint32_t div;

	regs->CR1 = 0;
	regs->CR2 = 0;
	regs->CR3 = 0;
	div = pclk / boudrate;
	regs->BRR = div;
	regs->CR1 = CR1_UE | CR1_TE | CR1_RE;
}

void usart_flush(uint8_t num)
{
	usart_regs_t *regs = get_usart_regs(num);

	while (!(regs->SR & SR_TC)) {
	}
}

static void _usart_send_byte(usart_regs_t *regs, uint8_t data)
{
	while (!(regs->SR & SR_TXE)) {
	}
	regs->DR = data;
}

void usart_send_byte(uint8_t num, uint8_t data)
{
	usart_regs_t *regs = get_usart_regs(num);

	_usart_send_byte(regs, data);
}

void usart_write(uint8_t num, void *ptr, int len)
{
	usart_regs_t *regs = get_usart_regs(num);

	for (int i = 0; i < len; i++)
		_usart_send_byte(regs, *(uint8_t *)(ptr + i));
}

static void _usart_putc(usart_regs_t *regs, char c)
{
	_usart_send_byte(regs, c);
	if (c == '\n')
		_usart_send_byte(regs, '\r');
}

void usart_putc(uint8_t num, char c)
{
	usart_regs_t *regs = get_usart_regs(num);

	_usart_putc(regs, c);
}

static void _usart_puts(usart_regs_t *regs, char *s)
{
	while (*s) {
		_usart_putc(regs, *s);
		s++;
	}
}

void usart_puts(uint8_t num, char *s)
{
	usart_regs_t *regs = get_usart_regs(num);

	_usart_puts(regs, s);
}

uint8_t usart_wait_and_recv_byte(uint8_t num)
{
	usart_regs_t *regs = get_usart_regs(num);

	while (!(regs->SR & SR_RXNE)) {
	}

	return regs->DR;
}

uint8_t usart_recv_byte(uint8_t num)
{
	usart_regs_t *regs = get_usart_regs(num);

	return regs->DR;
}

bool usart_is_received(uint8_t num)
{
	usart_regs_t *regs = get_usart_regs(num);

	return !!(regs->SR & SR_RXNE);
}

static void usart_puthex(usart_regs_t *regs, uint32_t hex, uint32_t digits)
{
	static const char tohex[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
					'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
	int start = 0;

	for (int i = 0; i < 8; i++) {
		if ((hex >> (4 * (7 - i))) & 0xF)
			break;
		start++;
	}
	if (digits < (8 - start))
		digits = 8 - start;

	start = 8 - digits;

	if (start > 7)
		start = 7;

	for (int i = start; i < 8; i++)
		_usart_putc(regs, tohex[(hex >> (4 * (7 - i))) & 0xF]);
}

static void usart_putint(usart_regs_t *regs, uint32_t data, int is_signed)
{
	char buf[12];
	int pos = sizeof(buf);
	int is_neg = is_signed && (((int32_t)data) < 0);

	buf[--pos] = '\0';
	if (data == 0) {
		buf[--pos] = '0';
	} else if (is_neg)
		data = -((int32_t)data);

	while (data) {
		buf[--pos] = '0' + (data % 10);
		data /= 10;
	}
	if (is_neg)
		buf[--pos] = '-';

	_usart_puts(regs, buf + pos);
}

void usart_printf(uint8_t num, char *s, ...)
{
	usart_regs_t *regs = get_usart_regs(num);
	uint32_t digits = 0;
	int is_percent_handler = 0;

	va_list args;
	va_start(args, s);
	while (*s) {
		if (*s == '%') {
			digits = 0;
			is_percent_handler = 1;
			s++;
			continue;
		}
		if (is_percent_handler) {
			switch (*s) {
			case '%':
				_usart_putc(regs, *s);
				is_percent_handler = 0;
				break;
			case '#':
				if (s[1] == 'x' || s[1] == 'X') {
					s++;
					_usart_puts(regs, "0x");
					usart_puthex(regs, va_arg(args, uint32_t), digits);
				} else {
					_usart_putc(regs, '%');
					_usart_putc(regs, '#');
				}
				is_percent_handler = 0;
				break;
			case 'x':
			case 'X':
				usart_puthex(regs, va_arg(args, uint32_t), digits);
				is_percent_handler = 0;
				break;
			case 'd':
			case 'i':
			case 'u':
				usart_putint(regs, va_arg(args, uint32_t), *s != 'u');
				is_percent_handler = 0;
				break;
			case 's':
				_usart_puts(regs, va_arg(args, char *));
				is_percent_handler = 0;
				break;
			default:
				if (*s >= '0' && *s <= '9') {
					digits = digits * 10 + (*s - '0');
				} else {
					_usart_putc(regs, '%');
					_usart_putc(regs, *s);
					is_percent_handler = 0;
				}
				break;
			}
			s++;
		} else
			_usart_putc(regs, *s++);
	}
	va_end(args);
}
