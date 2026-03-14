# FreeRTOS Spike+FYP Register Windows demo
# Build: riscv32-unknown-elf-gcc, run: spike --isa=rv32gc
# Run make from this directory (demo/Spike_FYP).

KERNEL := ../..
PORT   := $(KERNEL)/portable/GCC/RISC-V
CFLAGS  := -march=rv32gc -mabi=ilp32 -O1 -g -Wall -Wextra
CFLAGS  += -I. -I$(KERNEL)/include -I$(PORT)
CFLAGS  += -ffreestanding -nostdlib

# portContext.h includes the chip header via path relative to PORT when portasmSPIKE_FYP_CHIP is defined
ASFLAGS := -march=rv32gc -mabi=ilp32 -DportasmSPIKE_FYP_CHIP
ASFLAGS += -I$(abspath $(PORT)) -I.

LDFLAGS := -T link.ld -nostartfiles -nostdlib

PORT_C   := $(PORT)/port.c
PORT_S   := $(PORT)/portASM.S

OBJS := startup.o main.o libc_stubs.o
OBJS += tasks.o queue.o list.o
OBJS += port.o portASM.o

ELF := demo.elf

all: $(ELF)

$(ELF): $(OBJS)
	riscv32-unknown-elf-gcc $(CFLAGS) $(LDFLAGS) -o $@ $^

startup.o: startup.S
	riscv32-unknown-elf-gcc $(ASFLAGS) -c -o $@ $<

main.o: main.c
	riscv32-unknown-elf-gcc $(CFLAGS) -c -o $@ $<

libc_stubs.o: libc_stubs.c
	riscv32-unknown-elf-gcc $(CFLAGS) -c -o $@ $<

tasks.o: $(KERNEL)/tasks.c
	riscv32-unknown-elf-gcc $(CFLAGS) -c -o $@ $<

queue.o: $(KERNEL)/queue.c
	riscv32-unknown-elf-gcc $(CFLAGS) -c -o $@ $<

list.o: $(KERNEL)/list.c
	riscv32-unknown-elf-gcc $(CFLAGS) -c -o $@ $<

port.o: $(PORT)/port.c
	riscv32-unknown-elf-gcc $(CFLAGS) -c -o $@ $<

portASM.o: $(PORT)/portASM.S
	riscv32-unknown-elf-gcc $(ASFLAGS) -c -o $@ $<

run: $(ELF)
	spike --isa=rv32gc $(ELF)

clean:
	rm -f $(ELF) *.o

.PHONY: all run clean
