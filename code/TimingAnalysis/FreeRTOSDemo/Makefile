CC = riscv64-unknown-elf-gcc

ARCH ?= rv64gc
ABI ?= $(shell printf 'int x;' | $(CC) -x c -c -march=$(ARCH) -mabi=lp64 -o /dev/null - >/dev/null 2>&1 && echo lp64 || echo lp64d)

KERNEL_DIR ?= FreeRTOS-Kernel

CFLAGS = -march=$(ARCH) -mabi=$(ABI) \
         -mcmodel=medany \
         -O2 -g \
         -ffreestanding -fno-common -fno-builtin \
         -Wall -Wextra

LDFLAGS = -T linker.ld -nostdlib -static -Wl,--gc-sections

INCLUDES = \
    -I. \
    -I$(KERNEL_DIR)/include \
    -I$(KERNEL_DIR)/portable/GCC/RISC-V \
    -I$(KERNEL_DIR)/portable/GCC/RISC-V/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions

SOURCES = \
    startup.S \
    main.c \
    string.c \
    $(KERNEL_DIR)/tasks.c \
    $(KERNEL_DIR)/queue.c \
    $(KERNEL_DIR)/list.c \
    $(KERNEL_DIR)/portable/GCC/RISC-V/port.c \
    $(KERNEL_DIR)/portable/GCC/RISC-V/portASM.S \
    $(KERNEL_DIR)/portable/MemMang/heap_4.c

all: freertos.elf

freertos.elf: $(SOURCES) FreeRTOSConfig.h linker.ld
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) $(LDFLAGS) -o $@

clean:
	rm -f freertos.elf

run: freertos.elf
	spike --isa=rv64gc freertos.elf

.PHONY: all clean run
