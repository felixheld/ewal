################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../main.c \
../twimaster.c 

OBJS += \
./main.o \
./twimaster.o 

C_DEPS += \
./main.d \
./twimaster.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	avr-gcc -I/usr/local/CrossPack-AVR-20131216/avr/include -O3 -Wall -c -fmessage-length=0 -mmcu=attiny48 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


