PROJECT_NAME     := neodk
OUTPUT_DIRECTORY := build

STM32REPO := $(HOME)/STM32Cube/Repository
STM32CUBE_G0 := $(STM32REPO)/STM32Cube_FW_G0_V1.6.0
SEGGER := $(HOME)/SEGGER_RTT
PROJ_DIR_SRC := src
PROJ_DIR_INC := inc

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
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_gpio.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_exti.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_dac.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_adc.c \
  $(PROJ_DIR_SRC)/bsp_dbg_emb.c \
  $(SEGGER)/SEGGER_RTT.c \
  $(SEGGER)/SEGGER_RTT_printf.c \

# Source files common to all targets.
PROJ_COMMON_SRC += \
  $(PROJ_DIR_SRC)/neomain.c \
  $(PROJ_DIR_SRC)/convenience.c \

# Target-dependent include folders.
STM32G0xx_INC += \
  $(STM32CUBE_G0)/Drivers/CMSIS/Core/Include \
  $(STM32CUBE_G0)/Drivers/CMSIS/Device/ST/STM32G0xx/Include \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Inc \
  $(HOME)/gcc-arm/arm-none-eabi/include \
  $(SEGGER) \

# Include folders common to all targets
PROJ_COMMON_INC += \
  $(PROJ_DIR_INC) \

# Libraries common to all targets
LIB_FILES += \

# Optimization flags
# OPT = -O3 -g3
OPT = -O2
# Uncomment the line below to enable link time optimization
neodk_g071: OPT += -flto=auto
# Use newlib in nano version
neodk_g071: OPT += --specs=nano.specs -u _printf_float

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -Wall -Werror
# keep every function in a separate section, this allows the linker to discard unused ones.
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin # -fshort-enums

# Target-specific C flags.
neodk_g071: CFLAGS += -DSTM32G071xx
neodk_g071: CFLAGS += -mcpu=cortex-m0plus
neodk_g071: CFLAGS += -mfloat-abi=softfp
neodk_g071: CFLAGS += -DUSE_FULL_LL_DRIVER
neodk_g071: CFLAGS += -mthumb -mabi=aapcs

# C++ flags common to all targets
CXXFLAGS += $(OPT)

# Assembler flags common to all targets
ASMFLAGS += -g3

# Linker flags.
# Let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# Use newlib in nano version
neodk_g071: LDFLAGS += -u _printf_float

# Target-specific linker flags.
neodk_g071: LDFLAGS += $(OPT)
neodk_g071: LDFLAGS += -mthumb -mabi=aapcs -T$(LINKER_SCRIPT)
neodk_g071: LDFLAGS += -mcpu=cortex-m0plus
neodk_g071: LDFLAGS += -mfloat-abi=softfp

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

all: neodk_g071

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo		neodk_g071

TEMPLATE_PATH := toolchain/gcc
include $(TEMPLATE_PATH)/Makefile.common

neodk_g071:
  INC_FOLDERS := \
  $(STM32G0xx_INC) \
  $(PROJ_DIR_INC) \
  $(PROJ_COMMON_INC)
  SRC_FILES := \
  $(PROJ_DIR_SRC)/startup_stm32g071xx.s \
  $(PROJ_DIR_SRC)/bsp_stm32g071.c \
  $(STM32CUBE_G0)/Drivers/STM32G0xx_HAL_Driver/Src/stm32g0xx_hal_rcc_ex.c \
  $(STM32G0xx_SRC) \
  $(PROJ_COMMON_SRC)
  $(call define_target, neodk_g071)

#EOF
