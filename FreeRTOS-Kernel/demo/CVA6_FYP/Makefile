# FreeRTOS CVA6 register-window test suite.

KERNEL := ../..
PORT   := $(KERNEL)/portable/GCC/RISC-V
CHIP   := $(PORT)/chip_specific_extensions/CVA6_RV32_RegisterWindows

CROSS_COMPILE ?= riscv-none-elf-
CC := $(CROSS_COMPILE)gcc

# Match CVA6 bare-metal regressions by default; override if needed:
# make CORE_MARCH=rv32imac_zicsr_zifencei CORE_MABI=ilp32
CORE_MARCH ?= rv32imac_zbkb_zbkx_zkne_zknd_zknh_zicsr_zifencei
CORE_MABI ?= ilp32

CFLAGS  := -march=$(CORE_MARCH) -mabi=$(CORE_MABI) -O1 -g -Wall -Wextra
CFLAGS  += -I. -I$(KERNEL)/include -I$(PORT)
CFLAGS  += -ffreestanding -nostdlib

ASFLAGS := -march=$(CORE_MARCH) -mabi=$(CORE_MABI)
ASFLAGS += -I$(abspath $(PORT)) -I$(abspath $(CHIP)) -I.

LDFLAGS := -T link.ld -nostartfiles -nostdlib

COMMON_OBJS := startup.o libc_stubs.o tasks.o queue.o list.o port.o portASM.o

ALL_ELFS := demo_integrity.elf demo_sequence.elf demo_borrow.elf

all: $(ALL_ELFS)

demo_integrity.elf: integrity_test.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

demo_sequence.elf: sequence_test.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

demo_borrow.elf: borrow_test.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

startup.o: startup.S
	$(CC) $(ASFLAGS) -c -o $@ $<

libc_stubs.o: libc_stubs.c
	$(CC) $(CFLAGS) -c -o $@ $<

integrity_test.o: integrity_test.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

sequence_test.o: sequence_test.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

borrow_test.o: borrow_test.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

tasks.o: $(KERNEL)/tasks.c
	$(CC) $(CFLAGS) -c -o $@ $<

queue.o: $(KERNEL)/queue.c
	$(CC) $(CFLAGS) -c -o $@ $<

list.o: $(KERNEL)/list.c
	$(CC) $(CFLAGS) -c -o $@ $<

port.o: $(PORT)/port.c
	$(CC) $(CFLAGS) -c -o $@ $<

portASM.o: $(PORT)/portASM.S
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(ALL_ELFS)

.PHONY: all clean
