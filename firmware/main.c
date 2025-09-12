#include <stdint.h>
#include <string.h>

#include "stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_conf.h"

#include "adc.h"
#include "common.h"
#include "delay.h"
#include "flash.h"
#include "gpio.h"
#include "rcc.h"
#include "usart.h"


#define GPIO_LED_SMALL	GEN_GPIO(BANK_GPIOC, 13)
#define GPIO_STEP	GEN_GPIO(BANK_GPIOB, 1)
#define GPIO_DIR	GEN_GPIO(BANK_GPIOB, 0)
#define USART1_TX	GEN_GPIO(BANK_GPIOA, 9)
#define USART1_RX	GEN_GPIO(BANK_GPIOA, 10)
#define LED_ALARM	GEN_GPIO(BANK_GPIOB, 13)
#define GPIO_ENABLE	GEN_GPIO(BANK_GPIOA, 6)

#define UART_NUM 1

#define ADC_RUN_PERIOD		100
#define ADC_START_TIMEOUT	10

// Last page of flash
#define ENV_ADDR	0x0801fc00
#define ENV_MAGIC	0x564e45aa

#define ESC_UP		0x5b41
#define ESC_DOWN	0x5b42
#define ESC_RIGHT	0x5b43
#define ESC_LEFT	0x5b44

#define ESC_INS		0x5b327e
#define ESC_DEL		0x5b337e
#define ESC_PGUP	0x5b357e
#define ESC_PGDOWN	0x5b367e

#define ESC_END		0x5b46
#define ESC_END2	0x4f46
#define ESC_HOME	0x5b48
#define ESC_HOME2	0x5b317e

#define ESC_F1		0x4f50
#define ESC_F2		0x4f51

#define DEFAULT_ADC_OVEREMPTY	400
#define DEFAULT_ADC_EMPTY	800
#define DEFAULT_ADC_FULL	4000
#define DEFAULT_ADC_ALERT	1470
#define DEFAULT_STEPS_EMPTY	200
#define DEFAULT_STEPS_FULL	1550
#define DEFAULT_STEPS_TOTAL	2000
#define DEFAULT_USE_EMA_FILTER	1

#define ENV_ADC_OVEREMPTY	0
#define ENV_ADC_EMPTY		1
#define ENV_ADC_FULL		2
#define ENV_ADC_ALERT		3
#define ENV_STEPS_EMPTY		4
#define ENV_STEPS_FULL		5
#define ENV_STEPS_TOTAL		6
#define ENV_USE_EMA_FILTER	7

#define var_from_str(v, argv) uint32_t v; \
	do { \
		bool ok; \
		v = str_to_uint32(argv, &ok); \
		if (!ok) { \
			usart_printf(num, "Error: Argument '%s' is not an integer\n", argv); \
			return -1; \
		} \
	} while (0)


struct command {
	char *cmd;
	int (*func)(uint8_t num, int argc, char *argv[]);
	uint8_t arg_min;
	uint8_t arg_max;
	char *help;
};

struct env_record {
	char *name;
	uint32_t value;
	char *help;
};

struct motor {
	uint32_t current;
	uint32_t target;
	uint32_t set_dir_tick;
	uint32_t step_tick;
	bool step_is_high;
	bool dir_is_forward;
	bool is_debug;
};

struct adc {
	uint16_t values[100];
	uint32_t value;
	uint32_t run_tick;
	int values_pos;
	bool is_debug;
	bool is_run;
	bool is_started;
	bool is_values_wrapped;
};

struct console {
	char line[64];
	char history[4][64];
	int line_pos;
	int line_size;
	int history_pos;
	int history_size;
	int history_sel;
	bool is_esc_seq;
	uint32_t esc_seq;
	unsigned int esc_seq_pos;
};

int cmd_help(uint8_t num, int argc, char *argv[]);
int cmd_printenv(uint8_t num, int argc, char *argv[]);
int cmd_setenv(uint8_t num, int argc, char *argv[]);
int cmd_saveenv(uint8_t num, int argc, char *argv[]);
int cmd_loadenv(uint8_t num, int argc, char *argv[]);
int cmd_delenv(uint8_t num, int argc, char *argv[]);
int cmd_reset(uint8_t num, int argc, char *argv[]);
int cmd_get_adc(uint8_t num, int argc, char *argv[]);
int cmd_set_adc(uint8_t num, int argc, char *argv[]);
int cmd_debug_adc(uint8_t num, int argc, char *argv[]);
int cmd_debug_motor(uint8_t num, int argc, char *argv[]);
int cmd_get_motor(uint8_t num, int argc, char *argv[]);
int cmd_set_motor(uint8_t num, int argc, char *argv[]);
int cmd_adc_info(uint8_t num, int argc, char *argv[]);
int cmd_motor_info(uint8_t num, int argc, char *argv[]);
int cmd_park(uint8_t num, int argc, char *argv[]);

struct command cmds[] = {
	{ "help", cmd_help, 0, 0, },
	{ "printenv", cmd_printenv, 0, 1, "вывести все переменные окружения (если arg1 не указан) или вывести переменную, указанную в arg1", },
	{ "setenv", cmd_setenv, 2, 2, "указать новое целочисленное значение arg1 для переменной окружения arg1", },
	{ "saveenv", cmd_saveenv, 0, 0, "сохранить все переменные окружения во внутреннюю Flash-память", },
	{ "loadenv", cmd_loadenv, 0, 0, "загрузить ранее сохранённые переменные окружения", },
	{ "delenv", cmd_delenv, 0, 0, "очистить сохранённые переменные окружения во Flash-памяти... после перезагрузки будут использоваться занчения по умолчанию", },
	{ "reset", cmd_reset, 0, 0, "программная перезагрузка микроконтроллера", },
	{ "get_adc", cmd_get_adc, 0, 0, "вывести текущее значения АЦП", },
	{ "set_adc", cmd_set_adc, 1, 1, "изменить текущее значение АЦП на указанное в arg1 (смотри debug_adc)", },
	{ "debug_adc", cmd_debug_adc, 1, 1, "если arg1 != 0, то остановить работу АЦП... менять значения АЦП можно командой set_adc", },
	{ "debug_motor", cmd_debug_motor, 1, 1, "если arg1 != 0, то остановить автоматическое управление шаговым двигателем... требуется для set_motor", },
	{ "get_motor", cmd_get_motor, 0, 0, "вывести текущую позицию шагового двигателя", },
	{ "set_motor", cmd_set_motor, 1, 1, "указать текущую позицию шагового двигателя (смотри debug_motor)", },
	{ "adc_info", cmd_adc_info, 0, 0, "вывести полную информацию об АЦП", },
	{ "motor_info", cmd_motor_info, 0, 0, "вывести полную информацию об управлении шаговым двигателем", },
	{ "park", cmd_park, 1, 1, "парковка шагового двигателя в крайнее положене", },
};
struct console console;
struct motor motor;
struct adc adc;
struct env_record env[] = {
	{ "adc_overempty", DEFAULT_ADC_OVEREMPTY, "значение АЦП, до которого можно опускать стрелку", },
	{ "adc_empty", DEFAULT_ADC_EMPTY, "значение АЦП, соответствующее пустому баку", },
	{ "adc_full", DEFAULT_ADC_FULL, "значение АЦП, соответствующее полному баку", },
	{ "adc_alert", DEFAULT_ADC_ALERT, "значение АЦП, при котором горит светодиод", },
	{ "steps_empty", DEFAULT_STEPS_EMPTY, "количество шагов до отметки пустого бака", },
	{ "steps_full", DEFAULT_STEPS_FULL, "количество шагов до отметки полного бака", },
	{ "steps_total", DEFAULT_STEPS_TOTAL, "полное количество шагов до конца", },
	{ "use_ema_filter", DEFAULT_USE_EMA_FILTER, "0 - не фильтровать значения с АЦП, 1 - использовать фильтр EMA", },
};

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
	if (HAL_RCC_ClockConfig(&clkinitstruct, 2)!= HAL_OK)
		Error_Handler();
}

static uint32_t str_to_uint32(char *s, bool *ok)
{
	int base = 10;
	uint32_t value = 0;

	if (s[0] == '0') {
		switch (s[1]) {
		case 'x':
			base = 16;
			s = s + 2;
			break;
		case 'o':
			base = 8;
			s = s + 2;
			break;
		case 'b':
			base = 2;
			s = s + 2;
			break;
		default:
			break;
		}
	}

	while (*s) {
		uint8_t tmp = *s;

		value *= base;
		if (base == 16) {
			if (tmp >= 'A' && tmp <= 'F')
				tmp = tmp - 'A' + 10;
			else if (tmp >= 'a' && tmp <= 'f')
				tmp = tmp - 'a' + 10;
		} else
			tmp -= '0';

		if (tmp >= base) {
			if (ok)
				*ok = false;

			return value;
		}

		value += tmp;
		s++;
	}

	if (ok)
		*ok = true;

	return value;
}

static int env_load(void)
{
	int len;

	if (readl(ENV_ADDR) != ENV_MAGIC)
		return -1;

	len = readl(ENV_ADDR + 0x4);
	if (len < 0 || len > 128)
		return -1;

	if (ARRAY_SIZE(env) < len)
		len = ARRAY_SIZE(env);

	for (int i = 0; i < len; i++)
		env[i].value = readl(ENV_ADDR + 0x10 + (i * 0x4));

	return 0;
}

static int env_save(void)
{
	uint32_t data[ARRAY_SIZE(env) + 4];
	int res;
	int retry = 3;

	data[0] = ENV_MAGIC;
	data[1] = ARRAY_SIZE(env);
	data[2] = 0xffffffff;  // reserved
	data[3] = 0xffffffff;  // reserved
	for (int i = 0; i < ARRAY_SIZE(env); i++)
		data[i + 4] = env[i].value;

	res = flash_erase_page(ENV_ADDR);
	if (res)
		return res;

	do {
		retry--;
		res = flash_program(ENV_ADDR, data, sizeof(data));
		if (!res)
			res = flash_verify(ENV_ADDR, data, sizeof(data));
	} while (res && retry);

	return 0;
}

static void print_help_text(uint8_t num, char *s)
{
	int col = 4;

	usart_puts(num, "          ");
	while (*s) {
		if (((uint8_t)*s & 0xc0) != 0x80)  // if ASCII or first byte of UTF-8 symbol
			col++;

		if ((*s == ' ' && col >= 80) || (*s == '\n')) {
			usart_puts(num, "\n          ");
			col = 4;
		} else {
			usart_putc(num, *s);
		}

		s++;
	}
	usart_putc(num, '\n');
}

static void motor_set_dir(bool is_forward)
{
	motor.dir_is_forward = is_forward;
	motor.set_dir_tick = HAL_GetTick();
	gpio_pin_set(GPIO_DIR, (uint32_t)is_forward);
}

static void motor_set_step(bool high)
{
	motor.step_is_high = high;
	motor.step_tick = HAL_GetTick();
	gpio_pin_set(GPIO_STEP, (uint32_t)high);
	if (high)
		motor.current += motor.dir_is_forward ? 1 : -1;
}

static void motor_park(int steps)
{
	motor_set_dir(false);
	HAL_Delay(1);
	for (int i = 0; i < steps; i++) {
		motor_set_step(true);
		delay_us(400);
		motor_set_step(false);
		delay_us(400);
	}

	motor.current = 0;
}

int cmd_help(uint8_t num, int argc, char *argv[])
{
	usart_printf(num, "Доступные команды:\n");
	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		usart_printf(num, "  - %s", cmds[i].cmd);
		for (int j = 0; j < cmds[i].arg_min; j++)
			usart_printf(num, " <arg%d>", j + 1);

		for (int j = cmds[i].arg_min; j < cmds[i].arg_max; j++)
			usart_printf(num, " [arg%d]", j + 1);

		usart_putc(num, '\n');
		if (cmds[i].help)
			print_help_text(num, cmds[i].help);

		usart_putc(num, '\n');
	}

	return 0;
}

int cmd_printenv(uint8_t num, int argc, char *argv[])
{
	for (int i = 0; i < ARRAY_SIZE(env); i++) {
		if (argc && strcmp(argv[0], env[i].name))
			continue;

		usart_printf(num, "%s : %d  (%s)\n", env[i].name, env[i].value, env[i].help);
	}

	return 0;
}

int cmd_setenv(uint8_t num, int argc, char *argv[])
{
	for (int i = 0; i < ARRAY_SIZE(env); i++) {
		if (!strcmp(env[i].name, argv[0])) {
			var_from_str(value, argv[1]);

			env[i].value = value;

			return 0;
		}
	}

	usart_printf(num, "Error: Environment variable '%s' is not found\n", argv[0]);

	return -1;
}

int cmd_saveenv(uint8_t num, int argc, char *argv[])
{
	if (env_save()) {
		usart_puts(num, "Error: Failed to save environment\n");
		return -1;
	}

	return 0;
}

int cmd_loadenv(uint8_t num, int argc, char *argv[])
{
	if (env_load()) {
		usart_puts(num, "Error: Failed to load environment\n");
		return -1;
	}

	return 0;
}

int cmd_delenv(uint8_t num, int argc, char *argv[])
{
	if (flash_erase_page(ENV_ADDR)) {
		usart_puts(num, "Error: Failed to clear environment\n");
		return -1;
	}

	return 0;
}

int cmd_reset(uint8_t num, int argc, char *argv[])
{
	uint32_t value;

	usart_puts(num, "Resetting...\n");
	usart_flush(num);
	value = readl(0xe000ed0c);  // SCB_AIRCR register
	value = (value & 0xffff) | (0x5fa << 16);  // VECTKEY required to write to this register
	value |= BIT(2);  // SYSRESETREQ
	writel(value, 0xe000ed0c);

	return 0;
}

int cmd_get_adc(uint8_t num, int argc, char *argv[])
{
	usart_printf(num, "%d\n", adc.value);

	return 0;
}

int cmd_set_adc(uint8_t num, int argc, char *argv[])
{
	var_from_str(value, argv[0]);

	adc.value = value;
	gpio_pin_set(LED_ALARM, !!(adc.value < env[ENV_ADC_ALERT].value));


	return 0;
}

int cmd_debug_adc(uint8_t num, int argc, char *argv[])
{
	var_from_str(value, argv[0]);

	adc.is_debug = !!value;

	return 0;
}

int cmd_debug_motor(uint8_t num, int argc, char *argv[])
{
	var_from_str(value, argv[0]);

	motor.is_debug = !!value;

	return 0;
}

int cmd_get_motor(uint8_t num, int argc, char *argv[])
{
	usart_printf(num, "%d\n", motor.target);

	return 0;
}

int cmd_set_motor(uint8_t num, int argc, char *argv[])
{
	var_from_str(value, argv[0]);

	motor.target = value;

	return 0;
}

int cmd_adc_info(uint8_t num, int argc, char *argv[])
{
	uint32_t tick = HAL_GetTick();
	uint32_t pos;
	uint32_t count = 0;

	usart_printf(num, "value:      %u\n", adc.value);
	usart_printf(num, "values_pos: %d\n", adc.values_pos);
	usart_printf(num, "values:    ");
	pos = adc.values_pos;
	if (pos == 0)
		pos = ARRAY_SIZE(adc.values) - 1;
	else
		pos--;

	for (; pos > 0; pos--) {
		if (count && !(count & 0xf))
			usart_puts(num, "\n           ");

		count++;
		usart_printf(num, " %d", adc.values[pos]);
	}

	if (adc.is_values_wrapped && adc.values_pos) {
		pos = ARRAY_SIZE(adc.values) - 1;
		for (; pos > adc.values_pos; pos--) {
			if (count && !(count & 0xf))
				usart_puts(num, "\n           ");

			count++;
			usart_printf(num, " %d", adc.values[pos]);
		}
	}

	usart_putc(num, '\n');
	usart_printf(num, "run_tick:   %u (%u ms ago)\n", adc.run_tick, tick - adc.run_tick);
	usart_printf(num, "is_debug:   %u\n", (uint8_t)adc.is_debug);

	return 0;
}

int cmd_motor_info(uint8_t num, int argc, char *argv[])
{
	uint32_t tick = HAL_GetTick();

	usart_printf(num, "current:        %u\n", motor.current);
	usart_printf(num, "target:         %u\n", motor.target);
	usart_printf(num, "step_tick:      %u (%u ms ago)\n", motor.step_tick, tick - motor.step_tick);
	usart_printf(num, "set_dir_tick:   %u (%u ms ago)\n", motor.set_dir_tick, tick - motor.set_dir_tick);
	usart_printf(num, "step_is_high:   %u\n", (uint8_t)motor.step_is_high);
	usart_printf(num, "dir_is_forward: %u\n", (uint8_t)motor.dir_is_forward);
	usart_printf(num, "is_debug:       %u\n", (uint8_t)motor.is_debug);

	return 0;
}

int cmd_park(uint8_t num, int argc, char *argv[])
{
	var_from_str(value, argv[0]);

	motor_park(value);

	return 0;
}

void console_parse(uint8_t num)
{
	char *args[5];
	int argc = 0;
	int size = strlen(console.line);
	int pos = console.history_pos ? console.history_pos - 1 : ARRAY_SIZE(console.history) - 1;
	bool last_space = false;

	if (!size)
		return;

	if (strcmp(console.history[pos], console.line)) {
		strncpy(console.history[console.history_pos++], console.line, sizeof(console.history[0]));
		if (console.history_pos >= ARRAY_SIZE(console.history))
			console.history_pos = 0;

		if (console.history_size < ARRAY_SIZE(console.history))
			console.history_size++;
	}

	for (int i = 0; i < size; i++) {
		if (console.line[i] == ' ') {
			console.line[i] = '\0';
			last_space = true;
		} else {
			if (last_space) {
				if (argc >= ARRAY_SIZE(args)) {
					usart_puts(num, "Error: Too many arguments\n");
					return;
				}
				args[argc++] = &console.line[i];
				last_space = false;
			}
		}
	}

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		if (!strcmp(console.line, cmds[i].cmd)) {
			if (argc < cmds[i].arg_min || argc > cmds[i].arg_max) {
				usart_printf(num,
					     "Error: Incorrect arguments count. Expected [%d..%d], but sended %d\n",
					     cmds[i].arg_min,
					     cmds[i].arg_max,
					     argc);
				return;
			}

			cmds[i].func(num, argc, args);
			return;
		}
	}

	usart_printf(num, "Error: Unknown command '%s'\n", console.line);
}

static void console_move_cursor(uint8_t num, int shift, bool to_left)
{
	char buf[6] = "\x1b[0\0\0\0";
	char dir = to_left ? 'D' : 'C';

	if (!shift)
		return;
	else if (shift > 99)
		shift = 99;

	if (shift < 10) {
		buf[2] = shift + '0';
		buf[3] = dir;
	} else {
		buf[2] = shift / 10 + '0';
		buf[3] = shift % 10 + '0';
		buf[4] = dir;
	}

	usart_puts(num, buf);
}

static void console_print_history(uint8_t num)
{
	int len;
	int pos;

	if (console.history_sel) {
		pos = console.history_sel;
		if (pos > console.history_pos)
			pos = ARRAY_SIZE(console.history) - (pos - console.history_pos);
		else
			pos = console.history_pos - pos;

		strncpy(console.line, console.history[pos], sizeof(console.line));
	} else {
		console.line[0] = '\0';
	}

	len = strlen(console.line);
	console_move_cursor(num, console.line_pos, true);
	usart_puts(num, console.line);
	if (len < console.line_pos) {
		for (int i = 0; i < console.line_size - len; i++)
			usart_putc(num, ' ');

		console_move_cursor(num, console.line_size - len, true);
	}
	console.line_size = len;
	console.line_pos = len;
}

static void console_process(uint8_t num)
{
	while (usart_is_received(num)) {
		uint8_t ch = usart_recv_byte(num);

		if (console.is_esc_seq) {
			console.esc_seq = (console.esc_seq << 8) | ch;
			if ((console.esc_seq_pos && ch >= 0x41) || console.esc_seq_pos > 4 || ch <= 0x20) {
				console.is_esc_seq = false;
				switch (console.esc_seq) {
				case ESC_RIGHT:
					if (console.line_pos < console.line_size) {
						console_move_cursor(num, 1, false);
						console.line_pos++;
					}
					break;
				case ESC_LEFT:
					if (console.line_pos) {
						console_move_cursor(num, 1, true);
						console.line_pos--;
					}
					break;
				case ESC_HOME:
				case ESC_HOME2:
					if (console.line_pos) {
						console_move_cursor(num, console.line_pos, true);
						console.line_pos = 0;
					}
					break;
				case ESC_END:
				case ESC_END2:
					if (console.line_pos < console.line_size) {
						console_move_cursor(num, console.line_size - console.line_pos, false);
						console.line_pos = console.line_size;
					}
					break;
				case ESC_UP:
					if (console.history_sel < console.history_size) {
						console.history_sel++;
						console_print_history(num);
					}
					break;
				case ESC_DOWN:
					if (console.history_sel > 0) {
						console.history_sel--;
						console_print_history(num);
					}
					break;
				default:
					// usart_printf(num, " escseq: %#x\n", console.esc_seq);
					break;
				}
			} else {
				console.esc_seq_pos++;
			}

			continue;
		}

		switch (ch) {
		case 0x1b:  // Esc
			console.is_esc_seq = true;
			console.esc_seq = 0;
			console.esc_seq_pos = 0;
			break;
		case 0x8:  // Backspace
		case 0x7f:  // Backspace
			if (console.line_pos) {
				for (int i = console.line_pos; i < console.line_size + 1; i++)
					console.line[i - 1] = console.line[i];

				console.line_pos--;
				console.line_size--;
				console.line[console.line_size] = '\0';
				console_move_cursor(num, 1, true);
				usart_printf(num, "%s ", &console.line[console.line_pos]);
				console_move_cursor(num, console.line_size - console.line_pos + 1, true);
			}
			break;
		case 0xd:
			usart_printf(num, "\n");
			console_parse(num);
			usart_printf(num, "\n[console]# ");
			console.line_pos = 0;
			console.line_size = 0;
			console.line[0] = '\0';
			console.history_sel = 0;
		case 0xa:
			break;
		default:
			if (console.line_size < (sizeof(console.line) - 2)) {
				for (int i = console.line_size; i >= console.line_pos; i--)
					console.line[i + 1] = console.line[i];

				console.line[console.line_pos] = ch;
				usart_puts(num, &console.line[console.line_pos]);
				console_move_cursor(num, console.line_size - console.line_pos, true);
				console.line_size++;
				console.line_pos++;
			}
			break;
		}
	}
}

void adc_process(void)
{
	uint32_t tick = HAL_GetTick();
	int32_t svalue;
	uint32_t value;

	if (!adc.is_run && !adc.is_debug && (tick - adc.run_tick) > ADC_RUN_PERIOD) {
		adc_run_single(1, 4);
		adc.is_run = true;
		adc.is_started = false;
		adc.run_tick = tick;
	}

	if (adc.is_run && !adc.is_started) {
		adc.is_started = adc_is_started(1);
		if (!adc.is_started && (tick - adc.run_tick) > ADC_START_TIMEOUT) {
			// Error: ADC did not started
			// TODO: Stop ADC
			adc.is_run = false;
		}
	}

	if (adc.is_run && adc.is_started) {
		if (adc_is_completed(1)) {
			svalue = adc_get_result(1);
			adc.is_run = false;
			adc.is_started = false;
			if (svalue != -1) {
				if (env[ENV_USE_EMA_FILTER].value) {
					uint32_t n = ARRAY_SIZE(adc.values);
					uint32_t prev_idx = (adc.values_pos == 0) ?
							    (ARRAY_SIZE(adc.values) - 1) :
							    (adc.values_pos - 1);
					uint32_t prev = (adc.values_pos || adc.is_values_wrapped) ?
							adc.values[prev_idx] :
							svalue;

					// EMA filter (exponential moving average)
					svalue = (2 * svalue + ((n - 1) * prev)) / (n + 1);
				}

				adc.values[adc.values_pos++] = svalue;
				if (adc.values_pos >= ARRAY_SIZE(adc.values)) {
					adc.values_pos = 0;
					adc.is_values_wrapped = true;
				}

				value = 0;
				if (adc.is_values_wrapped) {
					for (int i = 0; i < ARRAY_SIZE(adc.values); i++)
						value += adc.values[i];
				} else {
					for (int i = 0; i < adc.values_pos; i++)
						value += adc.values[i];
				}

				adc.value = value / (adc.is_values_wrapped ? ARRAY_SIZE(adc.values) : adc.values_pos);
				gpio_pin_set(LED_ALARM, !!(adc.value < env[ENV_ADC_ALERT].value));
			} else {
				// Error: ADC is not ready... impossible here
				// TODO: Stop ADC
				(void)adc_get_result(1);
			}
		}
	}
}

static void calc_target(void)
{
	int32_t adc_value = adc.value;
	int32_t adc_range = env[ENV_ADC_FULL].value - env[ENV_ADC_EMPTY].value;
	int32_t step_range = env[ENV_STEPS_FULL].value - env[ENV_STEPS_EMPTY].value;
	int32_t adc_full_plus = env[ENV_ADC_FULL].value + (env[ENV_ADC_FULL].value / 10);

	if (adc_value < env[ENV_ADC_OVEREMPTY].value)
		adc_value = env[ENV_ADC_OVEREMPTY].value;
	else if (adc.value > adc_full_plus)
		adc_value = adc_full_plus;

	motor.target = ((adc_value - (int32_t)env[ENV_ADC_EMPTY].value) * step_range) / adc_range + (int32_t)env[ENV_STEPS_EMPTY].value;
}

#define MOTOR_HALF_PERIOD 2
#define MOTOR_DIR_TO_STEP_TIME 2

void motor_process(void)
{
	uint32_t tick = HAL_GetTick();

	if (!motor.is_debug) {
		if (adc.is_values_wrapped || adc.values_pos > 30)
		calc_target();
	}

	if (motor.step_is_high && (tick - motor.step_tick) >= MOTOR_HALF_PERIOD)
		motor_set_step(false);

	if (motor.current == motor.target)
		return;

	if (tick - motor.step_tick >= MOTOR_HALF_PERIOD) {
		bool is_forward = (motor.target > motor.current);
		if (is_forward != motor.dir_is_forward)
			motor_set_dir(is_forward);

		if (tick - motor.set_dir_tick >= MOTOR_DIR_TO_STEP_TIME) {
			motor_set_step(true);
		}
	}
}

int main(void)
{
	SystemClock_Config();
	SystemCoreClockUpdate();
	HAL_Init();

	rcc_clk_enable(RCC_CLK_GPIOA);
	rcc_clk_enable(RCC_CLK_GPIOB);
	rcc_clk_enable(RCC_CLK_GPIOC);
	rcc_clk_enable(RCC_CLK_ADC1);
	rcc_clk_enable(RCC_CLK_USART1);

	delay_init();

	gpio_init(USART1_TX, GPIO_DIR_OUT, GPIO_DRV_PP, GPIO_SPEED_MEDIUM, GPIO_FLAG_ALTERNATE);
	gpio_init(USART1_RX, GPIO_DIR_IN, GPIO_DRV_PP, GPIO_SPEED_MEDIUM, GPIO_FLAG_ALTERNATE);

	gpio_pin_set(GPIO_LED_SMALL, 1);
	gpio_init(GPIO_LED_SMALL, GPIO_DIR_OUT, GPIO_DRV_OD, GPIO_SPEED_LOW, 0);

	gpio_init(GPIO_STEP, GPIO_DIR_OUT, GPIO_DRV_PP, GPIO_SPEED_LOW, 0);
	gpio_init(GPIO_DIR, GPIO_DIR_OUT, GPIO_DRV_PP, GPIO_SPEED_LOW, 0);

	gpio_init(LED_ALARM, GPIO_DIR_OUT, GPIO_DRV_PP, GPIO_SPEED_LOW, 0);
	gpio_pin_set(LED_ALARM, 1);

	usart_init(UART_NUM, 72000000, 115200);

	gpio_init(GEN_GPIO(BANK_GPIOA, 4), GPIO_DIR_IN, GPIO_DRV_PP, GPIO_SPEED_MEDIUM, 0); // ADC12_IN4 - from fuel resistor
	gpio_init(GPIO_ENABLE, GPIO_DIR_IN, GPIO_DRV_PP, GPIO_SPEED_MEDIUM, 0); // ADC12_IN6 - ENABLE signal

	adc_init(1);
	adc_run_calibration(1);
	while (!adc_is_calibration_completed(1, NULL)) {
	}

	adc_set_sample_time(1, 10, ADC_SAMPLE_239_5_CYCLES);

	HAL_Delay(200);

	usart_printf(UART_NUM, "Environment load %s\n", env_load() ? "failed" : "done");
	cmd_printenv(UART_NUM, 0, NULL);

	usart_puts(UART_NUM, "Parking...\n");
	motor_park(env[ENV_STEPS_TOTAL].value);

	usart_puts(UART_NUM, "\n[console]# ");

	while (1) {
		console_process(UART_NUM);
		if (!gpio_pin_get(GPIO_ENABLE)) {
			motor_park(motor.current);
		} else {
			adc_process();
			motor_process();
		}
	}
}
