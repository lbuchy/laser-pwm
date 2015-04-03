cmake_minimum_required (VERSION 2.8.5)

SET(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake)

project(STM32TEST C CXX ASM)
ENABLE_LANGUAGE(ASM)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)


FUNCTION(PREPEND var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

##
## Setup the STM32 CUBE stuff
##
SET(STM32_CUBE_DIR ${CMAKE_CURRENT_BINARY_DIR}/STM32Cube_FW_F0_V1.2.0)
SET(STM32_CUBE_DRIVERS ${STM32_CUBE_DIR}/Drivers)
SET(STM32_CUBE_HAL ${STM32_CUBE_DRIVERS}/STM32F0xx_HAL_Driver)
# Add any additional driver files here
SET(STM32_CUBE_HAL_SRC_FILES_only
	"stm32f0xx_hal.c"
        "stm32f0xx_hal_wwdg.c"
	"stm32f0xx_hal_uart.c"
	"stm32f0xx_hal_gpio.c"
	"stm32f0xx_hal_rcc.c"
	"stm32f0xx_hal_rcc_ex.c"
	"stm32f0xx_hal_dma.c"
	"stm32f0xx_hal_tim.c"
        "stm32f0xx_hal_tim_ex.c"
	"stm32f0xx_hal_pwr.c"
	"stm32f0xx_hal_cortex.c"
	)

SET(STM32_CUBE_CMSIS_SRC_FILES_only
	"startup_stm32f030xc.s")

PREPEND(STM32_CUBE_HAL_SRC_FILES ${STM32_CUBE_HAL}/Src ${STM32_CUBE_HAL_SRC_FILES_only})
PREPEND(STM32_CUBE_CMSIS_SRC_FILES ${STM32_CUBE_DRIVERS}/CMSIS/Device/ST/STM32F0xx/Source/Templates/gcc ${STM32_CUBE_CMSIS_SRC_FILES_only})

add_definitions(-DSTM32F030xC)

SET(LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/flash.ld)
SET(CMAKE_EXE_LINKER_FLAGS "-T${LINKER_SCRIPT}")

# Find project include files
FILE(GLOB PROJECT_INCLUDE_FILES ${PROJECT_SOURCE_DIR}/Include/*.h)

SET(SRC 
        ${PROJECT_INCLUDE_FILES}
	"${PROJECT_SOURCE_DIR}/Source/main.c"
	"${PROJECT_SOURCE_DIR}/Source/stm32f0xx_hal_msp.c"
	"${PROJECT_SOURCE_DIR}/Source/stm32f0xx_it.c"
	"${PROJECT_SOURCE_DIR}/Source/system_stm32f0xx.c"
	"${STM32_CUBE_DRIVERS}/BSP/STM32F0308-Discovery/stm32f0308_discovery.c"
	)

SET(INC 
	"${PROJECT_SOURCE_DIR}/Include"
	"${STM32_CUBE_HAL}/Inc"
	"${STM32_CUBE_DRIVERS}/CMSIS/Device/ST/STM32F0xx/Include"
	"${STM32_CUBE_DRIVERS}/CMSIS/Include"
	"${STM32_CUBE_DRIVERS}/BSP/STM32F0308-Discovery"
	)
include_directories(${INC})

add_executable(firmware ${SRC} ${STM32_CUBE_HAL_SRC_FILES} ${STM32_CUBE_CMSIS_SRC_FILES})
