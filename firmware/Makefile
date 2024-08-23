#
#      Author  mark
#   Copyright  2023, 2024 Neostim
#

PROJECT_NAME     := neodk
OUTPUT_DIRECTORY := build

MAO_LIB_INC := maolib/inc
MAO_LIB_SRC := maolib/src
PROJ_DIR_INC := inc
PROJ_DIR_SRC := src
STM32REPO := $(HOME)/STM32Cube/Repository
STM32CUBE_G0 := $(STM32REPO)/STM32Cube_FW_G0_V1.6.0
SEGGER := $(HOME)/SEGGER_RTT

$(OUTPUT_DIRECTORY)/neodk_g071.out: \
  LINKER_SCRIPT := STM32G071_FLASH.ld

# Target-dependent source files
STM32G0xx_SRC := \
  $(STM32CUBE_G0)/Drivers/CMSIS/Device/ST/STM32G0xx/Source/Templates/system_stm32g0xx.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_cortex.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_pwr_ex.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc_ex.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_gpio.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_exti.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_dac.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_adc.c \
  $(SEGGER)/SEGGER_RTT.c \
  $(SEGGER)/SEGGER_RTT_printf.c \

# Source files common to all targets.
PROJ_COMMON_SRC += \
  $(PROJ_DIR_SRC)/neodk_main.c \
  $(PROJ_DIR_SRC)/controller.c \
  $(PROJ_DIR_SRC)/sequencer.c \
  $(PROJ_DIR_SRC)/pattern_iter.c \
  $(PROJ_DIR_SRC)/datalink.c \
  $(PROJ_DIR_SRC)/debug_cli.c \

# Target-dependent include folders.
STM32G0xx_INC += \
  $(STM32CUBE_G0)/Drivers/CMSIS/Core/Include \
  $(STM32CUBE_G0)/Drivers/CMSIS/Device/ST/STM32G0xx/Include \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Inc \
  $(HOME)/gcc-arm/arm-none-eabi/include \
  $(SEGGER) \

# Libraries common to all targets
LIB_FILES += -lmao \

# Optimization flags
# OPT = -O3 -g3
OPT = -O2
# Uncomment the line below to enable link time optimization
neodk_g071: OPT += -flto=auto
# Use newlib in nano version
neodk_g071: OPT += --specs=nano.specs # -u _printf_float

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -Wall -Werror
# keep every function in a separate section, this allows the linker to discard unused ones.
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin # -fshort-enums

# Target-specific C flags.
neodk_g071 libmao.a: CFLAGS += -DSTM32G071xx
neodk_g071 libmao.a: CFLAGS += -mcpu=cortex-m0plus
neodk_g071 libmao.a: CFLAGS += -mfloat-abi=softfp
neodk_g071 libmao.a: CFLAGS += -mthumb -mabi=aapcs
neodk_g071: CFLAGS += -DBUFFER_SIZE_UP=8192
neodk_g071: CFLAGS += -DUSE_FULL_LL_DRIVER

# C++ flags common to all targets
CXXFLAGS += $(OPT)

# Assembler flags common to all targets
ASMFLAGS += -g3

# Linker flags.
# Let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# LDFLAGS += --no-warn-rwx-segment
# Use newlib in nano version

# Target-specific linker flags.
neodk_g071: LDFLAGS += $(OPT)
neodk_g071: LDFLAGS += -mthumb -mabi=aapcs -T$(LINKER_SCRIPT)
neodk_g071: LDFLAGS += -mcpu=cortex-m0plus
neodk_g071: LDFLAGS += -mfloat-abi=softfp
neodk_g071: LDFLAGS += -Lmaolib

# Target-specific assembler flags.
neodk_g071: LDFLAGS += -mcpu=cortex-m0plus
neodk_g071: LDFLAGS += -mthumb -mabi=aapcs
neodk_g071: LDFLAGS += -mfloat-abi=softfp
neodk_g071: ASMFLAGS += -D__HEAP_SIZE=4096
neodk_g071: ASMFLAGS += -D__STACK_SIZE=4096

# Add standard libraries at the very end of the linker input,
# after all objects that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm

.PHONY: default help

# Default target - first one defined.
default: neodk_g071

libmao.a:
	$(CC) $(CFLAGS) -I$(MAO_LIB_INC) -I$(SEGGER) -c $(MAO_LIB_SRC)/*.c
	$(AR) maolib/$@ *.o
	rm *.o

all: neodk_g071

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo	neodk_g071
	@echo	libmao.a

TEMPLATE_PATH := toolchain/gcc
include $(TEMPLATE_PATH)/Makefile.common

neodk_g071:
  INC_FOLDERS := \
  $(STM32G0xx_INC) \
  $(PROJ_DIR_INC) \
  $(MAO_LIB_INC)
  SRC_FILES := \
  $(PROJ_DIR_SRC)/startup_stm32g071xx.s \
  $(PROJ_DIR_SRC)/bsp_stm32g071.c \
  $(STM32G0xx_SRC) \
  $(PROJ_COMMON_SRC)
  $(call define_target, neodk_g071)

#EOF
