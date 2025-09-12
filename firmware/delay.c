#include <stdint.h>

#include <cmsis_gcc.h>
#include <system_stm32f1xx.h>

#include "delay.h"
#include "common.h"

#define COREDEBUG_BASE_ADDR 0xE000EDF0
#define DWT_BASE_ADDR 0xE0001000

struct CoreDebug {
	volatile uint32_t DHCSR;                  /*!< Offset: 0x000 (R/W)  Debug Halting Control and Status Register */
	volatile uint32_t DCRSR;                  /*!< Offset: 0x004 ( /W)  Debug Core Register Selector Register */
	volatile uint32_t DCRDR;                  /*!< Offset: 0x008 (R/W)  Debug Core Register Data Register */
	volatile uint32_t DEMCR;                  /*!< Offset: 0x00C (R/W)  Debug Exception and Monitor Control Register */
};

struct DWT {
	volatile uint32_t CTRL;                   /*!< Offset: 0x000 (R/W)  Control Register */
	volatile uint32_t CYCCNT;                 /*!< Offset: 0x004 (R/W)  Cycle Count Register */
	volatile uint32_t CPICNT;                 /*!< Offset: 0x008 (R/W)  CPI Count Register */
	volatile uint32_t EXCCNT;                 /*!< Offset: 0x00C (R/W)  Exception Overhead Count Register */
	volatile uint32_t SLEEPCNT;               /*!< Offset: 0x010 (R/W)  Sleep Count Register */
	volatile uint32_t LSUCNT;                 /*!< Offset: 0x014 (R/W)  LSU Count Register */
	volatile uint32_t FOLDCNT;                /*!< Offset: 0x018 (R/W)  Folded-instruction Count Register */
	volatile uint32_t PCSR;                   /*!< Offset: 0x01C (R/ )  Program Counter Sample Register */
	volatile uint32_t COMP0;                  /*!< Offset: 0x020 (R/W)  Comparator Register 0 */
	volatile uint32_t MASK0;                  /*!< Offset: 0x024 (R/W)  Mask Register 0 */
	volatile uint32_t FUNCTION0;              /*!< Offset: 0x028 (R/W)  Function Register 0 */
	volatile uint32_t RESERVED0[1U];
	volatile uint32_t COMP1;                  /*!< Offset: 0x030 (R/W)  Comparator Register 1 */
	volatile uint32_t MASK1;                  /*!< Offset: 0x034 (R/W)  Mask Register 1 */
	volatile uint32_t FUNCTION1;              /*!< Offset: 0x038 (R/W)  Function Register 1 */
	volatile uint32_t RESERVED1[1U];
	volatile uint32_t COMP2;                  /*!< Offset: 0x040 (R/W)  Comparator Register 2 */
	volatile uint32_t MASK2;                  /*!< Offset: 0x044 (R/W)  Mask Register 2 */
	volatile uint32_t FUNCTION2;              /*!< Offset: 0x048 (R/W)  Function Register 2 */
	volatile uint32_t RESERVED2[1U];
	volatile uint32_t COMP3;                  /*!< Offset: 0x050 (R/W)  Comparator Register 3 */
	volatile uint32_t MASK3;                  /*!< Offset: 0x054 (R/W)  Mask Register 3 */
	volatile uint32_t FUNCTION3;              /*!< Offset: 0x058 (R/W)  Function Register 3 */
};

static uint32_t SystemCoreClock_in_KHz;
static uint32_t SystemCoreClock_in_MHz;

void delay_init(void) {
	// This is extremely weird: timer dwt doesn't start
	// without writing to CoreDebug->DEMCR.
	// Hint to write to CoreDebug->DEMCR was found in internet.
	// There is no documentation about CoreDebug->DEMCR in reference manual (ST document RM0008),
	// programming manual (ST document PM0214)

	struct CoreDebug *CoreDebug = (struct CoreDebug *)COREDEBUG_BASE_ADDR;
	CoreDebug->DEMCR |= BIT(24);  // TRCENA
	writel(readl(DWT_BASE_ADDR) | BIT(0), DWT_BASE_ADDR);  // CYCCNTENA
	SystemCoreClock_in_KHz = SystemCoreClock / 1000;
	SystemCoreClock_in_MHz = SystemCoreClock / 1000000;
}

uint32_t get_tick(void) {
	return readl(DWT_BASE_ADDR + 0x4);
}

uint32_t tick2ms(uint32_t tick) {
	return tick / SystemCoreClock_in_KHz;
}

uint32_t tick2us(uint32_t tick) {
	return tick / SystemCoreClock_in_MHz;
}

uint32_t ms2tick(uint32_t ms) {
	return ms * SystemCoreClock_in_KHz;
}

uint32_t us2tick(uint32_t us) {
	return us * SystemCoreClock_in_MHz;
}

void delay_us(uint32_t us) {
	__DMB();
	uint32_t start = readl(DWT_BASE_ADDR + 0x4);
	uint32_t finish = (us * (SystemCoreClock / 1000000)) + start;
	uint32_t tick;
	if (finish < start) {
		do {
			tick = readl(DWT_BASE_ADDR + 0x4);
		} while (tick >= start || tick <= finish);
	} else {
		do {
			tick = readl(DWT_BASE_ADDR + 0x4);
		} while (tick <= finish && tick >= start);
	}
	__DMB();
}

void delay_ms(uint32_t ms) {
	delay_us(ms * 1000);
}
