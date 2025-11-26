################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/bsp_lcd.c \
../Core/Src/bsp_rf.c \
../Core/Src/bsp_ui.c \
../Core/Src/bt_manager.c \
../Core/Src/debug_log.c \
../Core/Src/fsm_main.c \
../Core/Src/main.c \
../Core/Src/math_model.c \
../Core/Src/rf_measure.c \
../Core/Src/rf_trace.c \
../Core/Src/stm32l4xx_hal_msp.c \
../Core/Src/stm32l4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32l4xx.c \
../Core/Src/usb_cdc_bridge.c 

OBJS += \
./Core/Src/bsp_lcd.o \
./Core/Src/bsp_rf.o \
./Core/Src/bsp_ui.o \
./Core/Src/bt_manager.o \
./Core/Src/debug_log.o \
./Core/Src/fsm_main.o \
./Core/Src/main.o \
./Core/Src/math_model.o \
./Core/Src/rf_measure.o \
./Core/Src/rf_trace.o \
./Core/Src/stm32l4xx_hal_msp.o \
./Core/Src/stm32l4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32l4xx.o \
./Core/Src/usb_cdc_bridge.o 

C_DEPS += \
./Core/Src/bsp_lcd.d \
./Core/Src/bsp_rf.d \
./Core/Src/bsp_ui.d \
./Core/Src/bt_manager.d \
./Core/Src/debug_log.d \
./Core/Src/fsm_main.d \
./Core/Src/main.d \
./Core/Src/math_model.d \
./Core/Src/rf_measure.d \
./Core/Src/rf_trace.d \
./Core/Src/stm32l4xx_hal_msp.d \
./Core/Src/stm32l4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32l4xx.d \
./Core/Src/usb_cdc_bridge.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/bsp_lcd.cyclo ./Core/Src/bsp_lcd.d ./Core/Src/bsp_lcd.o ./Core/Src/bsp_lcd.su ./Core/Src/bsp_rf.cyclo ./Core/Src/bsp_rf.d ./Core/Src/bsp_rf.o ./Core/Src/bsp_rf.su ./Core/Src/bsp_ui.cyclo ./Core/Src/bsp_ui.d ./Core/Src/bsp_ui.o ./Core/Src/bsp_ui.su ./Core/Src/bt_manager.cyclo ./Core/Src/bt_manager.d ./Core/Src/bt_manager.o ./Core/Src/bt_manager.su ./Core/Src/debug_log.cyclo ./Core/Src/debug_log.d ./Core/Src/debug_log.o ./Core/Src/debug_log.su ./Core/Src/fsm_main.cyclo ./Core/Src/fsm_main.d ./Core/Src/fsm_main.o ./Core/Src/fsm_main.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/math_model.cyclo ./Core/Src/math_model.d ./Core/Src/math_model.o ./Core/Src/math_model.su ./Core/Src/rf_measure.cyclo ./Core/Src/rf_measure.d ./Core/Src/rf_measure.o ./Core/Src/rf_measure.su ./Core/Src/rf_trace.cyclo ./Core/Src/rf_trace.d ./Core/Src/rf_trace.o ./Core/Src/rf_trace.su ./Core/Src/stm32l4xx_hal_msp.cyclo ./Core/Src/stm32l4xx_hal_msp.d ./Core/Src/stm32l4xx_hal_msp.o ./Core/Src/stm32l4xx_hal_msp.su ./Core/Src/stm32l4xx_it.cyclo ./Core/Src/stm32l4xx_it.d ./Core/Src/stm32l4xx_it.o ./Core/Src/stm32l4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32l4xx.cyclo ./Core/Src/system_stm32l4xx.d ./Core/Src/system_stm32l4xx.o ./Core/Src/system_stm32l4xx.su ./Core/Src/usb_cdc_bridge.cyclo ./Core/Src/usb_cdc_bridge.d ./Core/Src/usb_cdc_bridge.o ./Core/Src/usb_cdc_bridge.su

.PHONY: clean-Core-2f-Src

