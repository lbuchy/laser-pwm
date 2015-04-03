INCLUDE(CMakeForceCompiler)

SET(TOOLCHAIN_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gcc-arm-none-eabi-4_9-2014q4)

SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
CMAKE_FORCE_C_COMPILER(${TOOLCHAIN_PREFIX}/bin/arm-none-eabi-gcc GNU)
CMAKE_FORCE_CXX_COMPILER(${TOOLCHAIN_PREFIX}/bin/arm-none-eabi-g++ GNU)

SET(OBJCOPY ${TOOLCHAIN_PREFIX}/bin/arm-none-eabi-objcopy)

SET(COMMON_FLAGS "-g -O0 -Wall -static-libgcc -mlittle-endian -mcpu=cortex-m0 -mthumb -mthumb-interwork -specs=nosys.specs")
SET(CMAKE_C_FLAGS "${COMMON_FLAGS} -std=c99")
SET(CMAKE_CXX_FLAGS "${COMMON_FLAGS}")
