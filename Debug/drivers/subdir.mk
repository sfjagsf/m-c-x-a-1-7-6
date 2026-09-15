################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../drivers/fsl_clock.c \
../drivers/fsl_common.c \
../drivers/fsl_common_arm.c \
../drivers/fsl_dac.c \
../drivers/fsl_edma.c \
../drivers/fsl_edma_soc.c \
../drivers/fsl_gpio.c \
../drivers/fsl_lpuart.c \
../drivers/fsl_lpuart_edma.c \
../drivers/fsl_reset.c \
../drivers/fsl_spc.c 

C_DEPS += \
./drivers/fsl_clock.d \
./drivers/fsl_common.d \
./drivers/fsl_common_arm.d \
./drivers/fsl_dac.d \
./drivers/fsl_edma.d \
./drivers/fsl_edma_soc.d \
./drivers/fsl_gpio.d \
./drivers/fsl_lpuart.d \
./drivers/fsl_lpuart_edma.d \
./drivers/fsl_reset.d \
./drivers/fsl_spc.d 

OBJS += \
./drivers/fsl_clock.o \
./drivers/fsl_common.o \
./drivers/fsl_common_arm.o \
./drivers/fsl_dac.o \
./drivers/fsl_edma.o \
./drivers/fsl_edma_soc.o \
./drivers/fsl_gpio.o \
./drivers/fsl_lpuart.o \
./drivers/fsl_lpuart_edma.o \
./drivers/fsl_reset.o \
./drivers/fsl_spc.o 


# Each subdirectory must supply rules for building sources it contributes
drivers/%.o: ../drivers/%.c drivers/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_MCXA176VLL -DCPU_MCXA176VLL_cm33 -DSDK_OS_BAREMETAL -DSERIAL_PORT_TYPE_UART=1 -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\drivers" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\component\lists" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\CMSIS" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\CMSIS\m-profile" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\device" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\device\periph5" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\component\uart" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\utilities" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\utilities\debug_console" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\component\serial_manager" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\utilities\str" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\utilities\debug_console\config" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\board" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\source" -O0 -fno-common -g3 -gdwarf-4 -Wall -c -ffunction-sections -fdata-sections -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-drivers

clean-drivers:
	-$(RM) ./drivers/fsl_clock.d ./drivers/fsl_clock.o ./drivers/fsl_common.d ./drivers/fsl_common.o ./drivers/fsl_common_arm.d ./drivers/fsl_common_arm.o ./drivers/fsl_dac.d ./drivers/fsl_dac.o ./drivers/fsl_edma.d ./drivers/fsl_edma.o ./drivers/fsl_edma_soc.d ./drivers/fsl_edma_soc.o ./drivers/fsl_gpio.d ./drivers/fsl_gpio.o ./drivers/fsl_lpuart.d ./drivers/fsl_lpuart.o ./drivers/fsl_lpuart_edma.d ./drivers/fsl_lpuart_edma.o ./drivers/fsl_reset.d ./drivers/fsl_reset.o ./drivers/fsl_spc.d ./drivers/fsl_spc.o

.PHONY: clean-drivers

