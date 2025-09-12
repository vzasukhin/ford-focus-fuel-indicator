CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
OD = $(CROSS_COMPILE)objdump
OC = $(CROSS_COMPILE)objcopy

CFLAGS = -mthumb -mcpu=cortex-m3 -g -fdata-sections -ffunction-sections -Os -std=gnu99 -Wall -DSTM32F103xB -DSTM32F1 -DUSE_STDPERIPH_DRIVER -Idrv/inc -I.

SRCS_C = \
adc_stm32f1.c \
flash_stm32f1.c \
gpio_stm32f1.c \
usart_stm32f1.c \
delay.c \
drv/src/system_stm32f1xx.c \
drv/src/stm32f1xx_hal.c \
drv/src/stm32f1xx_hal_cortex.c \
drv/src/stm32f1xx_hal_dma.c \
drv/src/stm32f1xx_hal_gpio.c \
drv/src/stm32f1xx_hal_rcc.c \
drv/src/stm32f1xx_hal_rcc_ex.c

SRCS_S = drv/src/startup_stm32f103xb.s

OBJS=$(SRCS_S:.S=.o)
OBJS+=$(SRCS_C:.c=.o)

compile: $(OBJS) main.o bootloader.o
	$(CC) -mcpu=cortex-m3 -mthumb -Os -T STM32F103XB_FLASH.ld -std=gnu99 -Wl,--gc-sections -Wl,-Map=test.map -Wl,--print-memory-usage -o test.elf $(OBJS) main.o
	$(CC) -mcpu=cortex-m3 -mthumb -Os -T bootloader.ld -std=gnu99 -Wl,--gc-sections -Wl,-Map=bootloader.map -Wl,--print-memory-usage -o bootloader.elf $(OBJS) bootloader.o
	$(OD) -S test.elf > test.dis
	$(OD) -s test.elf > test.dis2
	$(OC) -O binary test.elf test.bin
	$(OC) -O ihex test.elf test.hex

	$(OD) -S bootloader.elf > bootloader.dis
	$(OD) -s bootloader.elf > bootloader.dis2
	$(OC) -O binary bootloader.elf bootloader.bin
	$(OC) -O ihex bootloader.elf bootloader.hex

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.S.o:
	$(CC) -c $(CFLAGS) $< -o $@

all: compile flash

flash:
	stm32flash -w test.bin -v -S 0x08002000 /dev/ttyUSB0
	stm32flash -w bootloader.bin -v -g 0 /dev/ttyUSB0

clean:
	rm -rf *.o /drv/src/*.o test.elf test.bin test.dis test.dis2 test.hex test.map
