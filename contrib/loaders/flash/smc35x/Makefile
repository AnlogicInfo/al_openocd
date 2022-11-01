BIN2C = ../../../../src/helper/bin2char.sh

CROSS_COMPILE ?= riscv64-unknown-elf-
CROSS_COMPILE_AARCH ?= aarch64-none-elf-

RISCV_CC=$(CROSS_COMPILE)gcc
RISCV_OBJCOPY=$(CROSS_COMPILE)objcopy
RISCV_OBJDUMP=$(CROSS_COMPILE)objdump

AARCH_CC=$(CROSS_COMPILE_AARCH)gcc
AARCH_OBJCOPY=$(CROSS_COMPILE_AARCH)objcopy
AARCH_OBJDUMP=$(CROSS_COMPILE_AARCH)objdump

CFLAGS = -static -nostdlib -nostartfiles -Os -fPIC -g
RISCV32_CFLAGS = -march=rv32e -mabi=ilp32e $(CFLAGS)
RISCV64_CFLAGS = -march=rv64i -mabi=lp64 $(CFLAGS)
AARCH64_CFLAGS = -mabi=lp64 $(CFLAGS)

all: riscv32_smc35x.inc riscv64_smc35x.inc aarch64_smc35x.inc riscv64_smc35x.lst

.PHONY: clean

# .c -> .o
riscv32_%.o:  %.c
	$(RISCV_CC) -c $(RISCV32_CFLAGS) $< -o $@

riscv64_%.o:  %.c
	$(RISCV_CC) -c $(RISCV64_CFLAGS) $< -o $@

aarch64_%.o:  %.c
	$(AARCH_CC) -c $(AARCH64_CFLAGS) $< -o $@

# .S -> .o
riscv32_%.o:  riscv_%.S
	$(RISCV_CC) -c $(RISCV32_CFLAGS) $< -o $@

riscv64_%.o:  riscv_%.S
	$(RISCV_CC) -c $(RISCV64_CFLAGS) $< -o $@

aarch64_%.o:  aarch_%.S
	$(AARCH_CC) -c $(AARCH64_CFLAGS) $< -o $@

# .o -> .elf
riscv32_%.elf:	riscv32_%.o riscv32_wrapper.o
	$(RISCV_CC) -T riscv.lds $(RISCV32_CFLAGS) $^ -o $@

riscv64_%.elf:	riscv64_%.o riscv64_wrapper.o
	$(RISCV_CC) -T riscv.lds $(RISCV64_CFLAGS) $^ -o $@

aarch64_%.elf:	aarch64_%.o aarch64_wrapper.o
	$(AARCH_CC) -T aarch.lds $(AARCH64_CFLAGS) $^ -o $@

# utility
riscv%.lst: riscv%.elf
	$(RISCV_OBJDUMP) -S $< > $@

aarch%.lst: aarch%.elf
	$(AARCH_OBJDUMP) -S $< > $@

# .elf -> .bin
riscv%.bin: riscv%.elf
	$(RISCV_OBJCOPY) -Obinary $< $@

aarch%.bin: aarch%.elf
	$(AARCH_OBJCOPY) -Obinary $< $@

# .bin -> .inc
%.inc: %.bin
	$(BIN2C) < $< > $@

clean:
	-rm -f *.elf *.o *.lst *.bin *.inc