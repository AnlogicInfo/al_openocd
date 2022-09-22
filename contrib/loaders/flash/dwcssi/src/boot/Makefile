ARCH_SPLIT = $(subst _, ,$(ARCH))
ISA        = $(word 1,$(ARCH_SPLIT))

NAME      := boot
SRCS       = $(ISA)/wrapper.S
INC_PATH  += $(WORK_DIR)/src/boot

include $(WORK_DIR)/Makefile