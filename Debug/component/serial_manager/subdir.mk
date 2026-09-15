################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../component/serial_manager/fsl_component_serial_manager.c \
../component/serial_manager/fsl_component_serial_port_uart.c 

C_DEPS += \
./component/serial_manager/fsl_component_serial_manager.d \
./component/serial_manager/fsl_component_serial_port_uart.d 

OBJS += \
./component/serial_manager/fsl_component_serial_manager.o \
./component/serial_manager/fsl_component_serial_port_uart.o 


# Each subdirectory must supply rules for building sources it contributes
component/serial_manager/%.o: ../component/serial_manager/%.c component/serial_manager/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -D__REDLIB__ -DCPU_MCXA176VLL -DCPU_MCXA176VLL_cm33 -DSDK_OS_BAREMETAL -DSERIAL_PORT_TYPE_UART=1 -DSDK_DEBUGCONSOLE=1 -DCR_INTEGER_PRINTF -DPRINTF_FLOAT_ENABLE=0 -D__MCUXPRESSO -D__USE_CMSIS -DDEBUG -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\drivers" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\component\lists" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\CMSIS" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\CMSIS\m-profile" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\device" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\device\periph5" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\component\uart" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\utilities" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\utilities\debug_console" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\component\serial_manager" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\utilities\str" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\utilities\debug_console\config" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\board" -I"D:\MCUXpressoIDE_25.6.136\workspace\MCXA176_UartDmaDemo\source" -O0 -fno-common -g3 -gdwarf-4 -Wall -c -ffunction-sections -fdata-sections -fno-builtin -fmerge-constants -fmacro-prefix-map="$(<D)/"= -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -D__REDLIB__ -fstack-usage -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-component-2f-serial_manager

clean-component-2f-serial_manager:
	-$(RM) ./component/serial_manager/fsl_component_serial_manager.d ./component/serial_manager/fsl_component_serial_manager.o ./component/serial_manager/fsl_component_serial_port_uart.d ./component/serial_manager/fsl_component_serial_port_uart.o

.PHONY: clean-component-2f-serial_manager

