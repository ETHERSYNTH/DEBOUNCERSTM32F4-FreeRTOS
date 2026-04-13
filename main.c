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
#include "blue_task.h"
#include "red_task.h"
#include "orange_task.h"
#include "green_task.h"
//******************************************************************************
#define BUTTON_DEBOUNCE_MS    1
#define BUTTON_LONG_PRESS_MS  1000
#define BUTTON_DOUBLE_CLICK_MS 300
#define LEDTIMEMAX 350

#define STACK_SIZE_MIN	128	/* usStackDepth	- the stack size DEFINED IN WORDS.*/
#define tskBLUE_PRIORITY tskIDLE_PRIORITY + 1  

// Functions prototypes
extern void vLedBlinkBlue(void *pvParameters);
extern void vLedBlinkRed(void *pvParameters);
extern void vLedBlinkGreen(void *pvParameters);
extern void vLedBlinkOrange(void *pvParameters);

void Button_Init(void);
void EXTI_PA1_Init(void);
void DWT_Init(void);
void Button_ProcessingTask(void *pvParameters);
portTickType recalculateTime(portTickType timeActual, uint8_t* cnted_dwn);

// Global variabels (also for export)
portTickType ledtime = LEDTIMEMAX;
xQueueHandle xLEDMutex = NULL;
volatile portTickType responce_time;
volatile uint32_t start;

// Global variabels, static
static xTaskHandle xButtonTask = NULL;
static xTimerHandle xDebounceTimer = NULL;
static xQueueHandle xButtonQueue = NULL;


static portBASE_TYPE xLastButtonState = 0;  // GND (Input has external Pull-Down)
static volatile uint32_t ulButtonPressedCount = 0;

// User types
typedef enum{
    BUTTON_PRESSED,
    BUTTON_RELEASED,
    BUTTON_LONG_PRESSED,
    BUTTON_DOUBLE_CLICKED
}ButtonEventType_t;

typedef struct{
    ButtonEventType_t button_action;	// Action event type
    portTickType timestamp;      		// Time in ticks
    uint32_t press_count;        		// Button press counter
    uint8_t button_id;           		// ID, not used in case of one button 
}ButtonEvent_t;

//******************************************************************************
int main(void){
	RCC_ClocksTypeDef rcc_clocks;
	RCC_GetClocksFreq(&rcc_clocks);
	/*!< At this stage the microcontroller clock setting is already configured,
	   this is done through SystemInit() function which is called from startup
	   file (startup_stm32f4xx.s) before to branch to application main.
	   To reconfigure the default setting of SystemInit() function, refer to
	   system_stm32f4xx.c file
	 */
	
	/*!< Most systems default to the wanted configuration, with the noticeable 
		exception of the STM32 driver library. If you are using an STM32 with 
		the STM32 driver library then ensure all the priority bits are assigned 
		to be preempt priority bits by calling 
		NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 ); before the RTOS is started.
	*/

	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
	
	STM_EVAL_LEDInit(LED_BLUE);
	STM_EVAL_LEDInit(LED_RED);
	STM_EVAL_LEDInit(LED_ORANGE);
	STM_EVAL_LEDInit(LED_GREEN);
	
	Button_Init();
	EXTI_PA1_Init();

	xTaskCreate( vLedBlinkBlue, (const signed char*)"Led Blink Task Blue", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY, NULL );
	xTaskCreate( vLedBlinkRed, (const signed char*)"Led Blink Task Red", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY, NULL );
	xTaskCreate( vLedBlinkOrange, (const signed char*)"Led Blink Task Orange", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY, NULL );
	xTaskCreate( vLedBlinkGreen, (const signed char*)"Led Blink Task Green", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY, NULL );
	
	xLEDMutex = xQueueCreateMutex(queueQUEUE_TYPE_MUTEX);
	DWT_Init();
	vTaskStartScheduler();
}
//******************************************************************************
void EXTI_PA1_Init(void){
	
    GPIO_InitTypeDef GPIO_InitStruct;
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    // 1. Enable AHB&APB clocking (Advanced  High-performance & Peripherial buses)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);  //  GPIOA
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE); //  SYSCFG 

    // 2A. PA0 setup like input, not pulled
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN; 
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;  // No Pull (options: DOWN/UP/NOPULL) 
    GPIO_Init(GPIOA, &GPIO_InitStruct);

	// 2B. PA3 setup like output
    GPIO_InitStruct.GPIO_Pin = GPIO_EMUL_BT_OUT;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT; 
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;  // No Pull (options: DOWN/UP/NOPULL) 
    GPIO_Init(GPIOA, &GPIO_InitStruct);
	
    // 3. Link PA0 with EXTI Line
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);

    // 4. EXTI setup
    EXTI_InitStruct.EXTI_Line = EXTI_Line0;                			// EXTI Line 0
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;       			// 
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising_Falling;		// Both - Rising&Falling Edge
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;                 			// 
    EXTI_Init(&EXTI_InitStruct);									//

    // 5. NVIC
    //NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);      // Commented, because executed in main function

    NVIC_InitStruct.NVIC_IRQChannel = EXTI0_IRQn;          // Set IRQ chanel
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 5; // Example value = 2
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;        // Example value = 1 
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;           // Enable chanel
    NVIC_Init(&NVIC_InitStruct);	
}


void vDebounceTimerCallback(xTimerHandle xTimer){
    // Local variables
    uint8_t currentState = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
	ButtonEvent_t event;
    // Compare the actual and previous(last) state
    if (currentState != xLastButtonState){
        // Update last state
        xLastButtonState = currentState;
        
        if (currentState == (uint8_t)Bit_SET) {
            ulButtonPressedCount++;
            // Fill the even fields
            event.button_action = BUTTON_PRESSED,
            event.timestamp = xTaskGetTickCount(),
            event.press_count = ulButtonPressedCount;
			// Send event "PRESS" to queue
            xQueueSend(xButtonQueue, &event, 0);
			// Trigger a context switch immediately
			taskYIELD(); 
        }else{
			// Fill the even fields
            event.button_action = BUTTON_RELEASED,
            event.timestamp = xTaskGetTickCount();
			// Send event "RELEASE" to queue
            xQueueSend(xButtonQueue, &event, 0);
			// Trigger a context switch immediately
			taskYIELD(); 
        }
    }
}

void Button_ProcessingTask(void *pvParameters){
    ButtonEvent_t receivedEvent;
	static uint8_t cnted_dwn = 1;
	static volatile uint32_t cycles;
	static volatile uint32_t time_us;
    
    while(1) {
        if (xQueueReceive(xButtonQueue, &receivedEvent, portMAX_DELAY) == pdTRUE) {
            switch(receivedEvent.button_action) {
                case BUTTON_PRESSED:
					responce_time = xTaskGetTickCount() - responce_time;
					cycles = DWT->CYCCNT - start;
					time_us = cycles / 168;
					ledtime = recalculateTime(ledtime, &cnted_dwn);
                    break;
				
				case BUTTON_LONG_PRESSED:
                    // Reserved
                    break;
                    
                case BUTTON_RELEASED:
                    // Some action
                    break;
                    
                case BUTTON_DOUBLE_CLICKED:
                    // Reserved
                    break;
                    
                default:
                    break;
            }
        }
    }
}

void Button_Init(void){
    // Initialisation of timer and task to debounce and process push button
    xButtonQueue = xQueueCreate(10, sizeof(ButtonEvent_t));
    configASSERT(xButtonQueue != NULL);
	 
    xDebounceTimer = xTimerCreate(
        (const signed char *)"Debounce",
        pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS),
        pdFALSE,  // One-shot
        NULL,
        vDebounceTimerCallback
    );
    configASSERT(xDebounceTimer != NULL);
    
    xTaskCreate(
        Button_ProcessingTask,
        (const signed char *)"ButtonTask",
        configMINIMAL_STACK_SIZE + 128,
        NULL,
        tskIDLE_PRIORITY + 2,
        &xButtonTask
    );
}

void EXTI0_IRQHandler(void){
	typedef signed long  os_base_type_t;
	os_base_type_t higher_prio_woken = pdFALSE;
    // Check the line
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
		// Reset interrupt pending
        EXTI_ClearITPendingBit(EXTI_Line0);
		// Restart debounce timer
        xTimerResetFromISR(xDebounceTimer, &higher_prio_woken);
		// Trigger a context switch immediately
        portYIELD_FROM_ISR(higher_prio_woken);
    }
}

void DWT_Init(void){
    // Enable access to DWT
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Timer reset
    DWT->CYCCNT = 0;
    // Counter start
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

portTickType recalculateTime(portTickType timeActual, uint8_t* cnted_dwn){

	if(*cnted_dwn){
		if(timeActual > 50)
			timeActual -= 50;
		else{
			*cnted_dwn = 0;
			timeActual += 50;
		}
	}else{
		if(timeActual < LEDTIMEMAX)
			timeActual += 50;
		else{
			*cnted_dwn = 1;
			timeActual -= 50;
		}
	}
	return timeActual;
}
