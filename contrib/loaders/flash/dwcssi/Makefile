BIN2C = ../../../../src/helper/bin2char.sh

CROSS_COMPILE_RV ?= riscv64-unknown-elf-
CROSS_COMPILE_AARCH64 ?= aarch64-none-elf-

RISCV_CC=$(CROSS_COMPILE_RV)gcc
RISCV_OBJCOPY=$(CROSS_COMPILE_RV)objcopy
RISCV_OBJDUMP=$(CROSS_COMPILE_RV)objdump

AARCH64_CC=$(CROSS_COMPILE_AARCH64)gcc
AARCH64_OBJCOPY=$(CROSS_COMPILE_AARCH64)objcopy
AARCH64_OBJDUMP=$(CROSS_COMPILE_AARCH64)objdump

CFLAGS = -nostdlib -nostartfiles -Wall -Werror -Os -fPIC -Wunused-result -g
RISCV32_CFLAGS = -march=rv32e -mabi=ilp32e $(CFLAGS)
RISCV64_CFLAGS = -march=rv64i -mabi=lp64 $(CFLAGS)

all: riscv32_dwcssi.inc riscv64_dwcssi.inc riscv64_dwcssi.lst
riscv: riscv32_dwcssi.inc riscv64_dwcssi.inc riscv64_dwcssi.lst
aarch64: aarch64_dwcssi.inc aarch64_dwcssi.lst

.PHONY: clean

# .c -> .o
aarch64_%.o: aarch64_%.c
	$(AARCH64_CC) -c $(AARCH64_CFLAGS) $< -o $@ 

riscv32_%.o:  riscv_%.c
	$(RISCV_CC) -c $(RISCV32_CFLAGS) $^ -o $@

riscv64_%.o:  riscv_%.c
	$(RISCV_CC) -c $(RISCV64_CFLAGS) $< -o $@

# .S -> .o
aarch64_%.o:  aarch64_%.S
	$(AARCH64_CC) -c $(AARCH64_CFLAGS) $^ -o $@

riscv32_%.o:  riscv_%.S
	$(RISCV_CC) -c $(RISCV32_CFLAGS) $^ -o $@

riscv64_%.o:  riscv_%.S
	$(RISCV_CC) -c $(RISCV64_CFLAGS) $^ -o $@

# .o -> .elf
aarch64_%.elf:  aarch64_%.o aarch64_wrapper.o
	$(AARCH64_CC) -T aarch64.lds $(AARCH64_CFLAGS) $^ -o $@

riscv32_%.elf:	riscv32_%.o riscv32_wrapper.o
	$(RISCV_CC) -T riscv.lds $(RISCV32_CFLAGS) $^ -o $@

riscv64_%.elf:	riscv64_%.o riscv64_wrapper.o
	$(RISCV_CC) -T riscv.lds $(RISCV64_CFLAGS) $^ -o $@

# .elf -> .bin
aarch64%.bin: aarch64_%.elf
	$(AARCH64_OBJCOPY) -Obinary $< $@
%.bin: %.elf
	$(RISCV_OBJCOPY) -Obinary $< $@

# .bin -> .inc
%.inc: %.bin
	$(BIN2C) < $< > $@

# utility
aarch64_%.lst: aarch64_elf
	$(AARCH64_OBJDUMP) -S $< > $@

%.lst: %.elf
	$(RISCV_OBJDUMP) -S $< > $@

clean:
	-rm -f *.elf *.o *.lst *.bin *.inc
