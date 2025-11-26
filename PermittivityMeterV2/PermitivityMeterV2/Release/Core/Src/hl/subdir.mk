################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/hl/hal_pwm.c 

OBJS += \
./Core/Src/hl/hal_pwm.o 

C_DEPS += \
./Core/Src/hl/hal_pwm.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/hl/%.o Core/Src/hl/%.su Core/Src/hl/%.cyclo: ../Core/Src/hl/%.c Core/Src/hl/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-hl

clean-Core-2f-Src-2f-hl:
	-$(RM) ./Core/Src/hl/hal_pwm.cyclo ./Core/Src/hl/hal_pwm.d ./Core/Src/hl/hal_pwm.o ./Core/Src/hl/hal_pwm.su

.PHONY: clean-Core-2f-Src-2f-hl

