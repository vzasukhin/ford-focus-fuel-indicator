#ifndef _ADC_H
#define _ADC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef STM32F1
#define ADC_SAMPLE_1_5_CYCLES 0
#define ADC_SAMPLE_7_5_CYCLES 0x1
#define ADC_SAMPLE_13_5_CYCLES 0x2
#define ADC_SAMPLE_28_5_CYCLES 0x3
#define ADC_SAMPLE_41_5_CYCLES 0x4
#define ADC_SAMPLE_55_5_CYCLES 0x5
#define ADC_SAMPLE_71_5_CYCLES 0x6
#define ADC_SAMPLE_239_5_CYCLES 0x7
#endif

void adc_init(uint8_t num);
void adc_run_calibration(uint8_t num);
bool adc_is_calibration_completed(uint8_t num, uint16_t *codes);
void adc_set_sample_time(uint8_t num, uint32_t channel, uint32_t cycles);
void adc_run_single(uint8_t num, uint32_t channel);
bool adc_is_completed(uint8_t num);
bool adc_is_started(uint8_t num);
int32_t adc_get_result(uint8_t num);

#endif  // _ADC_H
