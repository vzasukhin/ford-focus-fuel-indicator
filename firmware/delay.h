#ifndef _DELAY_H
#define _DELAY_H

#include <stdint.h>

void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#endif  // _DELAY_H
