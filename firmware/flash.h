#ifndef _FLASH_H
#define _FLASH_H

#include <stdint.h>

int flash_erase_page(uintptr_t addr);
int flash_program(uintptr_t addr, void *ptr, unsigned int len);
int flash_verify(uintptr_t addr, void *ptr, unsigned int len);

#endif  // _FLASH_H
