#include <stdint.h>

#include <common.h>
#include <flash.h>
#include <usart.h>

#define FLASH_REGS_BASE_ADDR 0x40022000

#define REG_ACR		(FLASH_REGS_BASE_ADDR + 0)
#define REG_KEYR	(FLASH_REGS_BASE_ADDR + 0x4)
#define REG_OPTKEYR	(FLASH_REGS_BASE_ADDR + 0x8)
#define REG_SR		(FLASH_REGS_BASE_ADDR + 0xc)
#define REG_CR		(FLASH_REGS_BASE_ADDR + 0x10)
#define REG_AR		(FLASH_REGS_BASE_ADDR + 0x14)
#define REG_OBR		(FLASH_REGS_BASE_ADDR + 0x1c)
#define REG_WRPR	(FLASH_REGS_BASE_ADDR + 0x20)
#define REG_KEYR2	(FLASH_REGS_BASE_ADDR + 0x44)
#define REG_SR2		(FLASH_REGS_BASE_ADDR + 0x4c)
#define REG_CR2		(FLASH_REGS_BASE_ADDR + 0x50)
#define REG_AR2		(FLASH_REGS_BASE_ADDR + 0x54)

#define SR_EOP		BIT(5)
#define SR_WRPRTERR	BIT(4)
#define SR_PGERR	BIT(2)
#define SR_BSY		BIT(0)

#define CR_EOPIE	BIT(12)
#define CR_ERRIE	BIT(10)
#define CR_OPTWRE	BIT(9)
#define CR_LOCK		BIT(7)
#define CR_STRT		BIT(6)
#define CR_OPTER	BIT(5)
#define CR_OPTPG	BIT(4)
#define CR_MER		BIT(2)
#define CR_PER		BIT(1)
#define CR_PG		BIT(0)

#define KEY1	0x45670123
#define KEY2	0xcdef89ab
#define RDPRT	0xa5

static void flash_unlock(void)
{
	writel(KEY1, REG_KEYR);
	writel(KEY2, REG_KEYR);
}

static void flash_check_and_unlock(void)
{
	if (readl(REG_CR) & CR_LOCK)
		flash_unlock();
}

static void flash_lock(void)
{
	writel(CR_LOCK, REG_CR);
}

static uint32_t flash_wait_for_busy(void)
{
	uint32_t sr;
	uint32_t count = 10;

	do {
		sr = readl(REG_SR);
	} while (sr & SR_BSY);

	// Sometimes EOP bit is not set with BSY clear
	while (!(sr & SR_EOP) && count) {
		sr = readl(REG_SR);
	}

	return sr;
}

int flash_erase_page(uintptr_t addr)
{
	uint32_t sr;

	flash_check_and_unlock();
	writel(SR_EOP | SR_WRPRTERR | SR_PGERR, REG_SR);
	writel(CR_PER, REG_CR);
	writel(addr, REG_AR);
	writel(CR_PER | CR_STRT, REG_CR);
	sr = flash_wait_for_busy();
	flash_lock();

	return (sr & SR_EOP) ? 0 : -1;
}

int flash_verify(uintptr_t addr, void *ptr, unsigned int len)
{
	uint16_t *data = (uint16_t *)ptr;
	unsigned int pos = 0;

	for (pos = 0; pos < len; pos += 2) {
		if (read16(addr + pos) != data[pos >> 1])
			return -1;
	}

	return 0;
}

int flash_program(uintptr_t addr, void *ptr, unsigned int len)
{
	uint16_t *data = (uint16_t *)ptr;
	unsigned int pos = 0;
	uint32_t sr = 0;

	if (!len)
		return 0;

	flash_check_and_unlock();
	writel(CR_PG, REG_CR);
	for (pos = 0; pos < len; pos += 2) {
		writel(SR_EOP | SR_WRPRTERR | SR_PGERR, REG_SR);
		write16(data[pos >> 1], addr + pos);
		sr = flash_wait_for_busy();
		if (!(sr & SR_EOP))
			break;
	}

	flash_lock();

	return (sr & SR_EOP) ? 0 : -1;
}
