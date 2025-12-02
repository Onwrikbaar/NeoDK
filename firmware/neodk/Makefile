#
#  NOTICE (do not remove):
#      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
#      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
#
#   Author     mark
#   Copyright  2023..2026 Neostim™
#

MAO_LIB_INC := ../maolib/inc
PROJ_DIR_INC := inc
PROJ_DIR_SRC := src
STM32REPO := $(HOME)/STM32Cube/Repository
STM32CUBE_G0 ?= $(STM32REPO)/STM32Cube_FW_G0_V1.6.0
SEGGER ?= $(HOME)/SEGGER_RTT
OUTPUT_DIRECTORY := build

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
  $(PROJ_DIR_SRC)/patterns.c \
  $(PROJ_DIR_SRC)/pattern_iter.c \
  $(PROJ_DIR_SRC)/pulse_train.c \
  $(PROJ_DIR_SRC)/burst.c \
  $(PROJ_DIR_SRC)/datalink.c \
  $(PROJ_DIR_SRC)/debug_cli.c \
  $(PROJ_DIR_SRC)/attributes.c \

# Target-dependent include folders.
STM32G0xx_INC += \
  $(STM32CUBE_G0)/Drivers/CMSIS/Core/Include \
  $(STM32CUBE_G0)/Drivers/CMSIS/Device/ST/STM32G0xx/Include \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Inc \
  $(SEGGER) \

# Optimization flags.
# OPT = -O3 -g3
OPT = -O2
# Uncomment the line below to enable link time optimization.
OPT += -flto=auto
# Use newlib in nano version.
OPT += --specs=nano.specs

# C flags common to all targets.
CFLAGS += $(OPT)
CFLAGS += -Wall -Werror
# keep every function in a separate section, this allows the linker to discard unused ones.
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin # -fshort-enums

# Target-specific C flags.
CFLAGS += -DSTM32G071xx
CFLAGS += -mcpu=cortex-m0plus
CFLAGS += -mfloat-abi=softfp
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -DBUFFER_SIZE_UP=8192
CFLAGS += -DUSE_FULL_LL_DRIVER

# C++ flags common to all targets.
CXXFLAGS += $(OPT)

# Assembler flags common to all targets.
ASMFLAGS += -g3

# Linker flags.
# Let linker dump unused sections.
LDFLAGS += -Wl,--gc-sections
# LDFLAGS += --no-warn-rwx-segment

# Target-specific linker flags.
LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m0plus
LDFLAGS += -mfloat-abi=softfp
LDFLAGS += -L../maolib

# Target-specific assembler flags.
LDFLAGS += -mcpu=cortex-m0plus
LDFLAGS += -mthumb -mabi=aapcs
LDFLAGS += -mfloat-abi=softfp
ASMFLAGS += -D__HEAP_SIZE=4096
ASMFLAGS += -D__STACK_SIZE=4096

# Add standard libraries at the very end of the linker input,
# after all objects that may need symbols provided by these libraries.
LIB_FILES += -lmao_g071 -lc -lnosys -lm

.PHONY: default

# Default target - first one defined.
default: neodk_g071

TEMPLATE_PATH := ../toolchain/gcc
include $(TEMPLATE_PATH)/Makefile.common

neodk_g071:
  INC_FOLDERS := \
  $(STM32G0xx_INC) \
  $(PROJ_DIR_INC) \
  $(MAO_LIB_INC)
  SRC_FILES := \
  $(PROJ_DIR_SRC)/startup_stm32g071xx.s \
  $(PROJ_DIR_SRC)/bsp_stm32g071.c \
  $(PROJ_DIR_SRC)/bsp_uart_comms.c \
  $(STM32G0xx_SRC) \
  $(PROJ_COMMON_SRC)
  $(call define_target, neodk_g071)

#EOF
