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
#include "orange_task.h"
//******************************************************************************

extern portTickType ledtime;
extern xQueueHandle xLEDMutex;


void vLedBlinkOrange(void *pvParameters){
	for(;;)
	{
		xQueueTakeMutexRecursive(xLEDMutex, portMAX_DELAY);
			STM_EVAL_LEDToggle(LED_ORANGE);
			vTaskDelay( ledtime / portTICK_RATE_MS );
			STM_EVAL_LEDToggle(LED_ORANGE);
		xQueueGiveMutexRecursive(xLEDMutex);
	}
}
