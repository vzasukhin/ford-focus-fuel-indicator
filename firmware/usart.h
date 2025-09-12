#ifndef _USART_H
#define _USART_H

#include <stdbool.h>
#include <stdint.h>

void usart_init(uint8_t num, uint32_t pclk, uint32_t boudrate);

void usart_flush(uint8_t num);

// For binary data sends as is
void usart_send_byte(uint8_t num, uint8_t data);
void usart_write(uint8_t num, void *ptr, int len);

// For text data will correct end-of-line
void usart_putc(uint8_t num, char c);
void usart_puts(uint8_t num, char *s);
void usart_printf(uint8_t num, char *s, ...);

uint8_t usart_recv_byte(uint8_t num);
uint8_t usart_wait_and_recv_byte(uint8_t num);
bool usart_is_received(uint8_t num);

#endif  // _USART_H
