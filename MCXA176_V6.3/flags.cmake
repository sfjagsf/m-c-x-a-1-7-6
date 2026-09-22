if(NOT DEFINED FPU)
	set(FPU "-mfpu=fpv5-sp-d16 -mfloat-abi=hard")
endif()

if(${LIBRARY_TYPE} STREQUAL "REDLIB")
	set(SPECS "-specs=redlib.specs")
elseif(${LIBRARY_TYPE} STREQUAL "NEWLIB_NANO")
	set(SPECS "--specs=nano.specs")
endif()

if(NOT DEFINED DEBUG_CONSOLE_CONFIG)
	set(DEBUG_CONSOLE_CONFIG "-DSDK_DEBUGCONSOLE=1")
endif()

set(CMAKE_ASM_FLAGS_DEBUG " \
    ${CMAKE_ASM_FLAGS_DEBUG} \
    ${FPU} \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_C_FLAGS_DEBUG " \
    ${CMAKE_C_FLAGS_DEBUG} \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
    -DCPU_MCXA176VLL \
    -DCPU_MCXA176VLL_cm33 \
    -DSDK_OS_BAREMETAL \
    -DCR_INTEGER_PRINTF \
    -DPRINTF_FLOAT_ENABLE=0 \
    -DCMSIS_device_header=\\\"fsl_device_registers.h\\\" \
    -DRTE_COMPONENTS_H \
    -D__MCUXPRESSO \
    -D__USE_CMSIS \
    -DDEBUG \
    -DDISABLEFLOAT16 \
    -O0 \
    -fno-common \
    -fmerge-constants \
    -g3 \
     -ffunction-sections -fdata-sections -fno-builtin \
    -fstack-usage \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_CXX_FLAGS_DEBUG " \
    ${CMAKE_CXX_FLAGS_DEBUG} \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
    -O0 \
    -fno-common \
    -fmerge-constants \
    -g3 \
    -Wall \
    -fstack-usage \
    -mcpu=cortex-m33 \
    -mthumb \
")

set(CMAKE_EXE_LINKER_FLAGS_DEBUG " \
    ${CMAKE_EXE_LINKER_FLAGS_DEBUG} \
    ${FPU} \
    ${SPECS} \
    -nostdlib \
    -Xlinker \
    -Map=output.map \
    -Xlinker \
    --gc-sections \
    -Xlinker \
    -print-memory-usage \
    -Xlinker \
    --sort-section=alignment \
    -Xlinker \
    --cref \
    -mcpu=cortex-m33 \
    -mthumb \
    -T\"${ProjDirPath}/MCXA176_V6_3_Debug.ld\" \
    -L\"${ProjDirPath}/linkerScripts/Debug\" \
")
