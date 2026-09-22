# CROSS COMPILER SETTING
SET(CMAKE_SYSTEM_NAME Generic)
CMAKE_MINIMUM_REQUIRED (VERSION 3.10.0)

# THE VERSION NUMBER
SET (Tutorial_VERSION_MAJOR 1)
SET (Tutorial_VERSION_MINOR 0)

if (DEFINED ENV{SdkRootDirPath})
	SET(SdkRootDirPath "$ENV{SdkRootDirPath}")
endif()

if (NOT DEFINED SdkRootDirPath)
	MESSAGE(FATAL_ERROR "SDK path could not be determined. Please update SdkRootDirPath value from your CMake presets file(s).")
endif()

# ENABLE ASM
ENABLE_LANGUAGE(ASM)

# RESET FLAG VALUES
SET(CMAKE_ASM_FLAGS_DEBUG "")
SET(CMAKE_ASM_FLAGS_RELEASE "")
SET(CMAKE_C_FLAGS_DEBUG "")
SET(CMAKE_C_FLAGS_RELEASE "")
SET(CMAKE_CXX_FLAGS_DEBUG "")
SET(CMAKE_CXX_FLAGS_RELEASE "")
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "")

# CURRENT DIRECTORY
SET(ProjDirPath ${CMAKE_CURRENT_SOURCE_DIR})

SET(EXECUTABLE_OUTPUT_PATH ${ProjDirPath}/${CMAKE_BUILD_TYPE})
SET(LIBRARY_OUTPUT_PATH ${ProjDirPath}/${CMAKE_BUILD_TYPE})

set(CMAKE_USER_MAKE_RULES_OVERRIDE "user_override.cmake")

project(MCXA176_V6.3)
set(MCUX_SDK_PROJECT_NAME MCXA176_V6.3)

include(${ProjDirPath}/flags.cmake)

add_executable(${MCUX_SDK_PROJECT_NAME} "${ProjDirPath}/CMSIS/tz_context.h"
"${ProjDirPath}/CMSIS/core_cm33.h"
"${ProjDirPath}/CMSIS/cmsis_version.h"
"${ProjDirPath}/CMSIS/cmsis_gcc.h"
"${ProjDirPath}/CMSIS/cmsis_compiler.h"
"${ProjDirPath}/CMSIS/m-profile/cmsis_gcc_m.h"
"${ProjDirPath}/CMSIS/m-profile/armv8m_mpu.h"
"${ProjDirPath}/CMSIS/DSP/PrivateInclude/arm_vec_filtering.h"
"${ProjDirPath}/CMSIS/DSP/PrivateInclude/arm_vec_fft.h"
"${ProjDirPath}/CMSIS/DSP/PrivateInclude/arm_sorting.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_vec_math_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_vec_math.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_mve_tables_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_mve_tables.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_math_types_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_math_types.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_math_memory.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_math_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_math.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_helium_utils.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_const_structs_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_const_structs.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_common_tables_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/arm_common_tables.h"
"${ProjDirPath}/CMSIS/RTOS2/Source/os_systick.c"
"${ProjDirPath}/CMSIS/RTOS2/Include/os_tick.h"
"${ProjDirPath}/CMSIS/RTOS2/Include/cmsis_os2.h"
"${ProjDirPath}/CMSIS/DSP/Source/WindowFunctions/WindowFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/SVMFunctions/SVMFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/SVMFunctions/SVMFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/TransformFunctions/TransformFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/TransformFunctions/TransformFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/SupportFunctions/SupportFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/SupportFunctions/SupportFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/MatrixFunctions/MatrixFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/MatrixFunctions/MatrixFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/StatisticsFunctions/StatisticsFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/StatisticsFunctions/StatisticsFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/InterpolationFunctions/InterpolationFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/InterpolationFunctions/InterpolationFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/FastMathFunctions/FastMathFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/FastMathFunctions/FastMathFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/ControllerFunctions/ControllerFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/FilteringFunctions/FilteringFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/FilteringFunctions/FilteringFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/DistanceFunctions/DistanceFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/DistanceFunctions/DistanceFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/DistanceFunctions/arm_boolean_distance_template.h"
"${ProjDirPath}/CMSIS/DSP/Source/ComplexMathFunctions/ComplexMathFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/ComplexMathFunctions/ComplexMathFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/CommonTables/CommonTablesF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/CommonTables/CommonTables.c"
"${ProjDirPath}/CMSIS/DSP/Source/BayesFunctions/BayesFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/BayesFunctions/BayesFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Source/BasicMathFunctions/BasicMathFunctionsF16.c"
"${ProjDirPath}/CMSIS/DSP/Source/BasicMathFunctions/BasicMathFunctions.c"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/window_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/utils.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/transform_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/transform_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/svm_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/svm_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/svm_defines.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/support_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/support_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/statistics_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/statistics_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/quaternion_math_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/none.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/matrix_utils.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/matrix_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/matrix_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/interpolation_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/interpolation_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/filtering_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/filtering_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/fast_math_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/fast_math_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/distance_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/distance_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/debug.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/controller_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/controller_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/complex_math_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/complex_math_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/bayes_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/bayes_functions.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/basic_math_functions_f16.h"
"${ProjDirPath}/CMSIS/DSP/Include/dsp/basic_math_functions.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Include/rtx_os.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Include/rtx_evr.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Include/rtx_def.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Config/RTX_Config.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Config/RTX_Config.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_timer.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_thread.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_system.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_semaphore.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_mutex.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_msgqueue.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_mempool.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_memory.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_lib.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_lib.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_kernel.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_evr.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_evflags.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_delay.c"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_core_cm.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_core_ca.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/rtx_core_c.h"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source/GCC/irq_armv8mml.S"
"${ProjDirPath}/Modbus/Inc/ModbusTransport.h"
"${ProjDirPath}/Modbus/Inc/ModbusRtu.h"
"${ProjDirPath}/Modbus/Inc/ModbusMasterTransport.h"
"${ProjDirPath}/Modbus/Inc/ModbusMaster.h"
"${ProjDirPath}/Modbus/Inc/ModbusApp.h"
"${ProjDirPath}/Modbus/Src/ModbusTransport.c"
"${ProjDirPath}/Modbus/Src/ModbusRtu.c"
"${ProjDirPath}/Modbus/Src/ModbusMasterTransport.c"
"${ProjDirPath}/Modbus/Src/ModbusMaster.c"
"${ProjDirPath}/Modbus/Src/ModbusApp.c"
"${ProjDirPath}/SmartDMA_UART/app_smartdma_lpuart0.h"
"${ProjDirPath}/SmartDMA_UART/app_smartdma_lpuart0.c"
"${ProjDirPath}/SmartDMA_UART/app_smartdma_firmware.c"
"${ProjDirPath}/SmartDMA_UART/app_smartdma.h"
"${ProjDirPath}/SmartDMA_UART/app_smartdma.c"
"${ProjDirPath}/app/task/UartEchoTask.c"
"${ProjDirPath}/app/task/PwmDacTestTask.c"
"${ProjDirPath}/app/task/GpioInputTask.c"
"${ProjDirPath}/app/task/appStartup.c"
"${ProjDirPath}/app/include/WatchdogDriver.h"
"${ProjDirPath}/app/include/W25qxx.h"
"${ProjDirPath}/app/include/UartEchoTask.h"
"${ProjDirPath}/app/include/UartDriver.h"
"${ProjDirPath}/app/include/SpiTransport.h"
"${ProjDirPath}/app/include/Spi1PointDma.h"
"${ProjDirPath}/app/include/Spi1Digital.h"
"${ProjDirPath}/app/include/RtcDriver.h"
"${ProjDirPath}/app/include/PwmDriver.h"
"${ProjDirPath}/app/include/PwmDacTestTask.h"
"${ProjDirPath}/app/include/Pcf8563RtcDriver.h"
"${ProjDirPath}/app/include/MySpi.h"
"${ProjDirPath}/app/include/I2cEepromDriver.h"
"${ProjDirPath}/app/include/GpioInputTask.h"
"${ProjDirPath}/app/include/GpioDmaFilter.h"
"${ProjDirPath}/app/include/DacDriver.h"
"${ProjDirPath}/app/include/Crc16Driver.h"
"${ProjDirPath}/app/include/AdcDma.h"
"${ProjDirPath}/app/driver/WatchdogDriver.c"
"${ProjDirPath}/app/driver/W25qxx.c"
"${ProjDirPath}/app/driver/UartDriver.c"
"${ProjDirPath}/app/driver/SpiTransport.c"
"${ProjDirPath}/app/driver/Spi1PointDma.c"
"${ProjDirPath}/app/driver/Spi1Digital.c"
"${ProjDirPath}/app/driver/RtcDriver.c"
"${ProjDirPath}/app/driver/PwmDriver.c"
"${ProjDirPath}/app/driver/Pcf8563RtcDriver.c"
"${ProjDirPath}/app/driver/MySpi.c"
"${ProjDirPath}/app/driver/I2cEepromDriver.c"
"${ProjDirPath}/app/driver/GpioDmaFilter.c"
"${ProjDirPath}/app/driver/DacDriver.c"
"${ProjDirPath}/app/driver/Crc16Driver.c"
"${ProjDirPath}/app/driver/AdcDma.c"
"${ProjDirPath}/board/RTE_Device.h"
"${ProjDirPath}/board/pin_mux.h"
"${ProjDirPath}/board/pin_mux.c"
"${ProjDirPath}/board/peripherals.h"
"${ProjDirPath}/board/peripherals.c"
"${ProjDirPath}/board/clock_config.h"
"${ProjDirPath}/board/clock_config.c"
"${ProjDirPath}/board/board.h"
"${ProjDirPath}/board/board.c"
"${ProjDirPath}/component/lists/fsl_component_generic_list.h"
"${ProjDirPath}/component/lists/fsl_component_generic_list.c"
"${ProjDirPath}/component/gpio/fsl_adapter_gpio.h"
"${ProjDirPath}/component/gpio/fsl_adapter_gpio.c"
"${ProjDirPath}/device/system_MCXA176.h"
"${ProjDirPath}/device/system_MCXA176.c"
"${ProjDirPath}/device/MCXA176_features.h"
"${ProjDirPath}/device/MCXA176_COMMON.h"
"${ProjDirPath}/device/MCXA176.h"
"${ProjDirPath}/device/fsl_device_registers.h"
"${ProjDirPath}/device/periph5/PERI_WWDT.h"
"${ProjDirPath}/device/periph5/PERI_WUU.h"
"${ProjDirPath}/device/periph5/PERI_WAKETIMER.h"
"${ProjDirPath}/device/periph5/PERI_VBAT.h"
"${ProjDirPath}/device/periph5/PERI_UTICK.h"
"${ProjDirPath}/device/periph5/PERI_USB.h"
"${ProjDirPath}/device/periph5/PERI_UDF.h"
"${ProjDirPath}/device/periph5/PERI_TRNG.h"
"${ProjDirPath}/device/periph5/PERI_TRDC.h"
"${ProjDirPath}/device/periph5/PERI_SYSCON.h"
"${ProjDirPath}/device/periph5/PERI_SPC.h"
"${ProjDirPath}/device/periph5/PERI_SMARTDMA.h"
"${ProjDirPath}/device/periph5/PERI_SGI.h"
"${ProjDirPath}/device/periph5/PERI_SCG.h"
"${ProjDirPath}/device/periph5/PERI_RTC.h"
"${ProjDirPath}/device/periph5/PERI_PWM.h"
"${ProjDirPath}/device/periph5/PERI_PORT.h"
"${ProjDirPath}/device/periph5/PERI_PKC.h"
"${ProjDirPath}/device/periph5/PERI_OSTIMER.h"
"${ProjDirPath}/device/periph5/PERI_OPAMP.h"
"${ProjDirPath}/device/periph5/PERI_MRCC.h"
"${ProjDirPath}/device/periph5/PERI_LPUART.h"
"${ProjDirPath}/device/periph5/PERI_LPTMR.h"
"${ProjDirPath}/device/periph5/PERI_LPSPI.h"
"${ProjDirPath}/device/periph5/PERI_LPI2C.h"
"${ProjDirPath}/device/periph5/PERI_LPDAC.h"
"${ProjDirPath}/device/periph5/PERI_LPCMP.h"
"${ProjDirPath}/device/periph5/PERI_LCD.h"
"${ProjDirPath}/device/periph5/PERI_INPUTMUX.h"
"${ProjDirPath}/device/periph5/PERI_I3C.h"
"${ProjDirPath}/device/periph5/PERI_GPIO.h"
"${ProjDirPath}/device/periph5/PERI_GLIKEY.h"
"${ProjDirPath}/device/periph5/PERI_FREQME.h"
"${ProjDirPath}/device/periph5/PERI_FMU.h"
"${ProjDirPath}/device/periph5/PERI_FMC.h"
"${ProjDirPath}/device/periph5/PERI_FLEXIO.h"
"${ProjDirPath}/device/periph5/PERI_ERM.h"
"${ProjDirPath}/device/periph5/PERI_EQDC.h"
"${ProjDirPath}/device/periph5/PERI_EIM.h"
"${ProjDirPath}/device/periph5/PERI_DMA.h"
"${ProjDirPath}/device/periph5/PERI_DIGTMP.h"
"${ProjDirPath}/device/periph5/PERI_DEBUGMAILBOX.h"
"${ProjDirPath}/device/periph5/PERI_CTIMER.h"
"${ProjDirPath}/device/periph5/PERI_CRC.h"
"${ProjDirPath}/device/periph5/PERI_CMC.h"
"${ProjDirPath}/device/periph5/PERI_CDOG.h"
"${ProjDirPath}/device/periph5/PERI_CAN.h"
"${ProjDirPath}/device/periph5/PERI_AOI.h"
"${ProjDirPath}/device/periph5/PERI_ADC.h"
"${ProjDirPath}/drivers/fsl_wwdt.h"
"${ProjDirPath}/drivers/fsl_wwdt.c"
"${ProjDirPath}/drivers/fsl_spc.h"
"${ProjDirPath}/drivers/fsl_spc.c"
"${ProjDirPath}/drivers/fsl_smartdma_prv.h"
"${ProjDirPath}/drivers/fsl_smartdma.h"
"${ProjDirPath}/drivers/fsl_smartdma.c"
"${ProjDirPath}/drivers/fsl_rtc.h"
"${ProjDirPath}/drivers/fsl_rtc.c"
"${ProjDirPath}/drivers/fsl_reset.h"
"${ProjDirPath}/drivers/fsl_reset.c"
"${ProjDirPath}/drivers/fsl_pwm.h"
"${ProjDirPath}/drivers/fsl_pwm.c"
"${ProjDirPath}/drivers/fsl_port.h"
"${ProjDirPath}/drivers/fsl_lpuart_edma.h"
"${ProjDirPath}/drivers/fsl_lpuart_edma.c"
"${ProjDirPath}/drivers/fsl_lpuart.h"
"${ProjDirPath}/drivers/fsl_lpuart.c"
"${ProjDirPath}/drivers/fsl_lpspi.h"
"${ProjDirPath}/drivers/fsl_lpspi.c"
"${ProjDirPath}/drivers/fsl_lpi2c.h"
"${ProjDirPath}/drivers/fsl_lpi2c.c"
"${ProjDirPath}/drivers/fsl_lpadc.h"
"${ProjDirPath}/drivers/fsl_lpadc.c"
"${ProjDirPath}/drivers/fsl_gpio.h"
"${ProjDirPath}/drivers/fsl_gpio.c"
"${ProjDirPath}/drivers/fsl_flexio_mculcd.h"
"${ProjDirPath}/drivers/fsl_flexio_mculcd.c"
"${ProjDirPath}/drivers/fsl_flexio.h"
"${ProjDirPath}/drivers/fsl_flexio.c"
"${ProjDirPath}/drivers/fsl_edma_soc.h"
"${ProjDirPath}/drivers/fsl_edma_soc.c"
"${ProjDirPath}/drivers/fsl_edma_core.h"
"${ProjDirPath}/drivers/fsl_edma.h"
"${ProjDirPath}/drivers/fsl_edma.c"
"${ProjDirPath}/drivers/fsl_dac.h"
"${ProjDirPath}/drivers/fsl_dac.c"
"${ProjDirPath}/drivers/fsl_ctimer.h"
"${ProjDirPath}/drivers/fsl_ctimer.c"
"${ProjDirPath}/drivers/fsl_crc.h"
"${ProjDirPath}/drivers/fsl_crc.c"
"${ProjDirPath}/drivers/fsl_common_arm.h"
"${ProjDirPath}/drivers/fsl_common_arm.c"
"${ProjDirPath}/drivers/fsl_common.h"
"${ProjDirPath}/drivers/fsl_common.c"
"${ProjDirPath}/drivers/fsl_clock.h"
"${ProjDirPath}/drivers/fsl_clock.c"
"${ProjDirPath}/drivers/mcxa/fsl_smartdma_fw.h"
"${ProjDirPath}/drivers/mcxa/fsl_smartdma_fw.c"
"${ProjDirPath}/source/semihost_hardfault.c"
"${ProjDirPath}/source/MCXA176_V6.3.c"
"${ProjDirPath}/startup/startup_MCXA176.c"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set_target_properties(${MCUX_SDK_PROJECT_NAME} PROPERTIES SUFFIX ".axf")
endif()

if(${LIBRARY_TYPE} STREQUAL "REDLIB")
    target_include_directories(${MCUX_SDK_PROJECT_NAME} PRIVATE $ENV{ARMGCC_DIR}/redlib/include)
endif()

set(asm_include_directories "${ProjDirPath}/drivers"
"${ProjDirPath}/component/lists"
"${ProjDirPath}/CMSIS"
"${ProjDirPath}/CMSIS/m-profile"
"${ProjDirPath}/device"
"${ProjDirPath}/device/periph5"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Include"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Config"
"${ProjDirPath}/CMSIS/DSP/Include"
"${ProjDirPath}/CMSIS/DSP/PrivateInclude"
"${ProjDirPath}/CMSIS/DSP/Source/DistanceFunctions"
"${ProjDirPath}/CMSIS/RTOS2/Include"
"${ProjDirPath}/drivers/mcxa"
"${ProjDirPath}/component/gpio"
"${ProjDirPath}/board"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source"
"${ProjDirPath}/freertos/freertos-kernel/include"
"${ProjDirPath}/source"
"${ProjDirPath}/app/include"
"${ProjDirPath}/Modbus/Inc"
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:${asm_include_directories}>)

set(c_include_directories "${ProjDirPath}/drivers"
"${ProjDirPath}/component/lists"
"${ProjDirPath}/CMSIS"
"${ProjDirPath}/CMSIS/m-profile"
"${ProjDirPath}/device"
"${ProjDirPath}/device/periph5"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Include"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Config"
"${ProjDirPath}/CMSIS/DSP/Include"
"${ProjDirPath}/CMSIS/DSP/PrivateInclude"
"${ProjDirPath}/CMSIS/DSP/Source/DistanceFunctions"
"${ProjDirPath}/CMSIS/RTOS2/Include"
"${ProjDirPath}/drivers/mcxa"
"${ProjDirPath}/component/gpio"
"${ProjDirPath}/board"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Source"
"${ProjDirPath}/freertos/freertos-kernel/include"
"${ProjDirPath}/source"
"${ProjDirPath}/app/include"
"${ProjDirPath}/Modbus/Inc"
)

target_include_directories(${MCUX_SDK_PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:C>:${c_include_directories}>)

set(cpp_include_directories "")

target_include_directories(${MCUX_SDK_PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${cpp_include_directories}>)

set(include_files "")

target_precompile_headers(${MCUX_SDK_PROJECT_NAME} PRIVATE ${include_files})

set(link_directories "${ProjDirPath}/CMSIS/RTOS2"
"${ProjDirPath}/CMSIS/RTOS2/RTX/Library/GCC"
)

set(link_libraries "")

target_link_directories(${MCUX_SDK_PROJECT_NAME} PRIVATE ${link_directories})
target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE -Wl,--start-group)
target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE ${TARGET_LINK_SYSTEM_LIBRARIES} ${link_libraries})
target_link_libraries(${MCUX_SDK_PROJECT_NAME} PRIVATE -Wl,--end-group)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	add_custom_command(
		TARGET ${MCUX_SDK_PROJECT_NAME}
		POST_BUILD
		COMMAND $ENV{ARMGCC_DIR}/bin/arm-none-eabi-size "$<TARGET_FILE_NAME:${MCUX_SDK_PROJECT_NAME}>" || true
	)
endif()

