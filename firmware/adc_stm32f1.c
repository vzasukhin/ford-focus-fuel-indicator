#include <adc.h>
#include <common.h>

#define ADC1_BASE_ADDR 0x40012400
#define ADC2_BASE_ADDR 0x40012800
#define ADC3_BASE_ADDR 0x40013c00

#define CR1_AWDEN BIT(23)
#define CR1_JAWDEN BIT(22)
#define CR1_DUALMOD(x) ((x) << 16)
#define CR1_DISCNUM(x) ((x) << 13)
#define CR1_JDISCEN BIT(12)
#define CR1_DISCEN BIT(11)
#define CR1_JAUTO BIT(10)
#define CR1_AWDSGL BIT(9)
#define CR1_SCAN BIT(8)
#define CR1_JEOCIE BIT(7)
#define CR1_AWDIE BIT(6)
#define CR1_EOCIE BIT(5)
#define CR1_AWDCH(x) ((x) << 0)

#define CR2_TSVREFE BIT(23)
#define CR2_SWSTART BIT(22)
#define CR2_JSWSTART BIT(21)
#define CR2_EXTTRIG BIT(20)
#define CR2_EXTSEL(x) ((x) << 17)
#define CR2_JEXTTRIG BIT(15)
#define CR2_JEXTSEL(x) ((x) << 12)
#define CR2_ALIGN BIT(11)
#define CR2_DMA BIT(8)
#define CR2_RSTCAL BIT(3)
#define CR2_CAL BIT(2)
#define CR2_CONT BIT(1)
#define CR2_ADON BIT(0)

#define SR_STRT BIT(4)
#define SR_JSTRT BIT(3)
#define SR_JEOC BIT(2)
#define SR_EOC BIT(1)
#define SR_AWD BIT(0)

typedef struct {
	volatile uint32_t SR;
	volatile uint32_t CR1;
	volatile uint32_t CR2;
	volatile uint32_t SMPR1;
	volatile uint32_t SMPR2;
	volatile uint32_t JOFR[4];
	volatile uint32_t HTR;
	volatile uint32_t LTR;
	volatile uint32_t SQR1;
	volatile uint32_t SQR2;
	volatile uint32_t SQR3;
	volatile uint32_t JSQR;
	volatile uint32_t JDR[4];
	volatile uint32_t DR;
} adc_regs_t;

static adc_regs_t *get_adc_regs(uint8_t num)
{
	switch (num) {
	case 1:
		return (adc_regs_t *)ADC1_BASE_ADDR;
	case 2:
		return (adc_regs_t *)ADC2_BASE_ADDR;
	case 3:
		return (adc_regs_t *)ADC3_BASE_ADDR;
	default:
		return NULL;
	}
}

void adc_init(uint8_t num)
{
	adc_regs_t *regs = get_adc_regs(num);

	regs->CR1 = CR1_DISCNUM(0) | CR1_DISCEN;

	// power on ADC only if ADON is not set. If ADON is set then set ADON will start measure
	if (!(regs->CR2 & CR2_ADON))
		regs->CR2 = CR2_ADON;

}

void adc_run_calibration(uint8_t num)
{
	adc_regs_t *regs = get_adc_regs(num);

	regs->CR2 |= CR2_CAL;
}

bool adc_is_calibration_completed(uint8_t num, uint16_t *codes)
{
	adc_regs_t *regs = get_adc_regs(num);
	uint32_t data;

	if (regs->CR2 & CR2_CAL)
		return false;

	data = regs->DR;
	if (codes)
		*codes = data;

	return true;
}

void adc_set_sample_time(uint8_t num, uint32_t channel, uint32_t cycles)
{
	adc_regs_t *regs = get_adc_regs(num);
	uint32_t shift;
	uint32_t mask;
	
	if (channel >= 10) {
		shift = (channel - 10) * 3;
		mask = 0x7 << shift;
		regs->SMPR1 = (regs->SMPR1 & (~mask)) | (cycles << shift);
	} else {
		shift = channel * 3;
		mask = 0x7 << shift;
		regs->SMPR2 = (regs->SMPR2 & (~mask)) | (cycles << shift);
	}
}

void adc_run_single(uint8_t num, uint32_t channel)
{
	adc_regs_t *regs = get_adc_regs(num);

	regs->SQR1 = 0;  // 1 conversion
	regs->SQR2 = 0;
	regs->SQR3 = channel & 0x1f;
	regs->SR = 0;  // clear all status flags
	regs->CR2 |= CR2_ADON;  // start conversion
}

bool adc_is_completed(uint8_t num)
{
	adc_regs_t *regs = get_adc_regs(num);

	return !!(regs->SR & SR_EOC);
}

bool adc_is_started(uint8_t num)
{
	adc_regs_t *regs = get_adc_regs(num);

	return !!(regs->SR & SR_STRT);
}

int32_t adc_get_result(uint8_t num)
{
	adc_regs_t *regs = get_adc_regs(num);
	
	if (!(regs->SR & SR_EOC))
		return -1;  // conversion is not completed

	return regs->DR & 0xffff;
}
