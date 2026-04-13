//******************************************************************************
#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"
#include "discoveryf4utils.h"
//******************************************************************************
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "croutine.h"
#include "timers.h"
#include "portmacro.h"
//#include "projdefs.h"
#include "mpu_wrappers.h"
#include "green_task.h"
//******************************************************************************

extern portTickType ledtime;
extern xQueueHandle xLEDMutex;
extern volatile portTickType responce_time;
extern volatile uint32_t start;


void vLedBlinkGreen(void *pvParameters){
	for(;;)
	{
		xQueueTakeMutexRecursive(xLEDMutex, portMAX_DELAY);
			STM_EVAL_LEDToggle(LED_GREEN);
			vTaskDelay( ledtime / portTICK_RATE_MS );
			STM_EVAL_LEDToggle(LED_GREEN);
			responce_time = xTaskGetTickCount();
			start = DWT->CYCCNT;
			GPIO_SetBits(GPIOA, GPIO_EMUL_BT_OUT);
			vTaskDelay( 100 / portTICK_RATE_MS );
			GPIO_ResetBits(GPIOA, GPIO_EMUL_BT_OUT);
		xQueueGiveMutexRecursive(xLEDMutex);
	}
}
