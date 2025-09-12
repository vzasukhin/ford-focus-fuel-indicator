#ifndef _COMMON_H
#define _COMMON_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define BIT(x) (1 << (x))
// #define GENMASK(h, l) (((1 << ((h) - (l) + 1)) - 1) << (l))
// #define __bf_shf(x) (__builtin_ffsll(x) - 1)
// #define FIELD_PREP(_mask, _val) (((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask))
// #define FIELD_GET(_mask, _reg) ((typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask)))

struct time {
	uint16_t msec;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	bool is_changed;
	bool is_valid;
};

inline static uint32_t readl(uintptr_t addr)
{
	return *(volatile uint32_t *)(addr);
}

inline static void writel(uint32_t val, uintptr_t addr)
{
	*(volatile uint32_t *)(addr) = val;
}

inline static uint16_t read16(uintptr_t addr)
{
	return *(volatile uint16_t *)(addr);
}

inline static void write16(uint16_t val, uintptr_t addr)
{
	*(volatile uint16_t *)(addr) = val;
}

inline static uint8_t read8(uintptr_t addr)
{
	return *(volatile uint8_t *)(addr);
}

inline static void write8(uint8_t val, uintptr_t addr)
{
	*(volatile uint8_t *)(addr) = val;
}

#endif
