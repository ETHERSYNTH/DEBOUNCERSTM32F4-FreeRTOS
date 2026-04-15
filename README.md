stm32f4-discovery-keil-freertos-blink
====================================

Sysclk 168 MHz

Project on Keil (MDK-ARM) for stm32f4-discovery + FreeRTOS 7.5.2

Led blinks one by one, clockwise. BLUE -> RED -> ORANG -> GREEN

There are tasks for each color LED control + push button control task with debounce filter via timer callback. 

Emulation of push button "B1"(PA0 input, external) to connect with PA3 output.

#define BUTTON_DEBOUNCE_MS    1

After GREEN led off activate push button to measuring responce time.
By  BUTTON_PRESSED event happend increment/decrement leding time.
