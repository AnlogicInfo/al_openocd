.PHONY: all run gdb clean latest $(ALL)


ifeq ($(MAKECMDGOALS),)
  MAKECMDGOALS  = image
  .DEFAULT_GOAL = image
endif


## arch init
BIN2C = ../../../../../src/helper/bin2char.sh
ARCH_SPLIT = $(subst _, ,$(ARCH))
ISA        = $(word 1,$(ARCH_SPLIT))
PLATFORM   = $(word 2,$(ARCH_SPLIT))


## dir config
WORK_DIR  = $(shell pwd)
SRC_DIR   = $(WORK_DIR)/src
DST_DIR   = $(WORK_DIR)/build/$(ARCH)
$(info # dst dir $(DST_DIR))
$(shell mkdir -p $(DST_DIR))

IMAGE_REL = build/emmc_$(ARCH)
IMAGE     = $(abspath $(IMAGE_REL))


SRCS		 	+= $(WORK_DIR)/src/dwcmshc.c \
				   $(WORK_DIR)/src/boot/$(ISA)/wrapper.S

				
OBJS 		   	= $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(SRCS))))

$(info # OBJS $(OBJS))
LD_SCRIPT		= $(WORK_DIR)/scripts/$(ISA).lds


## comilation config
ifeq ($(ISA),aarch)
	CROSS_COMPILE := aarch64-none-elf-
	COMMON_FLAGS  := -mabi=lp64
else
	CROSS_COMPILE := riscv-none-embed-
	ifeq ($(PLATFORM),32)
		COMMON_FLAGS  := -march=rv32e -mabi=ilp32e
	else
		COMMON_FLAGS  := -march=rv64i -mabi=lp64
	endif
endif

AS        = $(CROSS_COMPILE)gcc
CC        = $(CROSS_COMPILE)gcc
CXX       = $(CROSS_COMPILE)g++
LD        = $(CROSS_COMPILE)ld
OBJDUMP   = $(CROSS_COMPILE)objdump
OBJCOPY   = $(CROSS_COMPILE)objcopy
READELF   = $(CROSS_COMPILE)readelf


INC_PATH += $(WORK_DIR)/src/boot/$(ISA)
INCFLAGS += $(addprefix -I, $(INC_PATH))

CFLAGS   += $(COMMON_FLAGS) -nostdlib -fno-builtin  -nostartfiles -Wall -Werror  -Os -fPIC -Wunused-result -g $(INCFLAGS)
ASFLAGS  += $(COMMON_FLAGS) -MMD $(INCFLAGS)

$(info # Compiler $(CROSS_COMPILE))

$(DST_DIR)/%.o: %.c
	@mkdir -p $(dir $@) && echo + CC $<
	@$(CC) $(CFLAGS) -c -o $@ $(realpath $<)

$(DST_DIR)/%.o: %.S
	@mkdir -p $(dir $@) && echo + AS $<
	@$(CC) $(CFLAGS) -c -o $@ $(realpath $<)

$(IMAGE).elf: $(OBJS)
	$(CC) -T $(LD_SCRIPT) $(CFLAGS) -o $(IMAGE).elf $(OBJS)


$(IMAGE).bin: $(IMAGE).elf
	@$(OBJDUMP) -D $(IMAGE).elf > $(IMAGE).txt
	@$(OBJCOPY) -S -Obinary $(IMAGE).elf $(IMAGE).bin

$(IMAGE).inc: $(IMAGE).bin
	$(BIN2C) < $< > $@

image: $(IMAGE).inc
	rm -rf $(DST_DIR)

clean:
	rm -rf $(WORK_DIR)/build
