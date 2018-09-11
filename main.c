/********************************************************************************
  * @file    project/main.c 
  * @author  Mohammed Al-Sanabani
	// NOTE: Explanation for the calculations are inline
  * @version V1.0.0
  * @date    5-april-2014
  * @brief   Main program body
  ******************************************************************************/
#include "main.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ioe.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "portmacro.h"
#include "cpu_utils.h"
#include "task.h"
#include <stdio.h>
#include "semphr.h"
#include "PCP_mutex.h"

// used to read the timer directly from memory 
volatile uint32_t * TIM2timerAddress = (uint32_t *)0x40000024;

// constant for 45M loops
static const uint32_t oneSecondLoops = 45000000;

// line number to print to
// LCD is 240x320
unsigned int lineNo = 319;

// used to calculate average lock time
unsigned int timesWeLocked = 0;
unsigned int timesWeUnlocked = 0;

// required to turn on/off the LED4 (red LED)
GPIO_InitTypeDef GPIO_InitStructure;

/* ===== TASK 0  ==== */
// task handle
xTaskHandle t0Handle = NULL;
// stack size and name
#define t0STACK   ( 512 )
#define t0Priority (tskIDLE_PRIORITY + 10)
static const char * t0Name = "Task0";

/* ===== TASK 1  ==== */
// task handle
xTaskHandle t1Handle = NULL;
// stack size and name
#define t1STACK   ( 512 )
static const char * t1Name = "Task1";
#define t1Priority (tskIDLE_PRIORITY + 9)

/* ===== TASK 2  ==== */
// task handle
xTaskHandle t2Handle = NULL;
// stack size and name
#define t2STACK   ( 512 )
static const char * t2Name = "Task2";
#define t2Priority (tskIDLE_PRIORITY + 8)

/* ===== TASK 3  ==== */
// task handle
xTaskHandle t3Handle = NULL;
// stack size and name
#define t3STACK   ( 512 )
static const char * t3Name = "Task3";
#define t3Priority (tskIDLE_PRIORITY + 7)

/* ===== TASK 4  ==== */
// task handle
xTaskHandle t4Handle = NULL;
// stack size and name
#define t4STACK   ( 512 )
static const char * t4Name = "Task4";
#define t4Priority (tskIDLE_PRIORITY + 6)

// task used to print average times to screen
// done at end. Task has lowest priority
/* ===== TASK 5  ==== */
// task handle
xTaskHandle t5Handle = NULL;
// stack size and name
#define t5STACK   ( 512 )
static const char * t5Name = "Task5";
#define t5Priority (tskIDLE_PRIORITY + 5)

// CREATE BINARY SEMAPHORE / Mutex ARRAY
xSemaphoreHandle freeRTOSMutexArray[5];


 /* A function to initialize the TIM2 and TIM5 General purpose 32Bit timers */
 /* TIM2 timer is used to count the duriation of the two second delay, 
    and the TIM5 Timer is used for generating interrupts every 10ms */
void InitializeTimers()
{
   /* The TIM (General Purpose 32bit Timer) structure
	* prescaler set to zero, period set to max (32 bit)
	* for the TIM2.
	* clock divider set to zero, and will only count 
	* once (repetition counter = 0)
	* TIM2 is used for average locking time
	* TIM3 is used for average unlocking time
	* TIM5 is used for interrupt
	
	*/
	TIM_TimeBaseInitTypeDef timerInitStructure; 
	timerInitStructure.TIM_Prescaler              = 0;
	timerInitStructure.TIM_CounterMode            = TIM_CounterMode_Up;
	timerInitStructure.TIM_Period                 = 0xFFFFFFFF;
	timerInitStructure.TIM_ClockDivision          = TIM_CKD_DIV1;
	timerInitStructure.TIM_RepetitionCounter      = 0;
 /* Enable the Low Speed APB (APB1) peripheral clock.
	* After reset, the peripheral clock (used for register read/write access)
	* is disabled and the application software has to enable this clock before 
	* using it.
	*/	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

  /*
	* Initialize the TIM2 (General Purpose 32bit Timer) with
	* the timerInitStructure from above
	*/
	TIM_TimeBaseInit(TIM2, &timerInitStructure);	
	TIM_TimeBaseInit(TIM3, &timerInitStructure);	

	// for TIM5, we want every 10ms an interrupt, therefore
	// every (90MHz * 0.01) - 1 clocks (first clock increments 
	// counter from 0). Period = 89.999K (0xDBB9F)
	// multiply by 10 to get 100ms interrupts (explain)
	//timerInitStructure.TIM_Period                 = 0xDBB9F;
	// Update: Make interrupt every 100ms to print slower to screen
	// so multiply .TIM_Period by 10.
	// this gives us ~32 seconds to print to screen (since its
	// 320 pixels wide and we print to one line of pixels per
	// interrupt
	timerInitStructure.TIM_Period                 = 0x895436;
	TIM_TimeBaseInit(TIM5, &timerInitStructure);
}

void InitializeLCD()
{
	/* LCD initialization */
  LCD_Init();
  
  /* LCD Layer initialization */
  LCD_LayerInit();
    
  /* Enable the LTDC */
  LTDC_Cmd(ENABLE);
  
  /* Set LCD foreground layer */
  LCD_SetLayer(LCD_FOREGROUND_LAYER);
	
	/* Clear the LCD */ 
  LCD_Clear(LCD_COLOR_WHITE);
  
	/* Configure the IO Expander */
  if (IOE_Config() == IOE_OK)
  {   
    LCD_SetFont(&Font8x8);
	  LCD_SetTextColor(LCD_COLOR_BLACK); 
	}
	else
	{
		LCD_SetFont(&Font8x8);
	  LCD_Clear(LCD_COLOR_RED);
    LCD_SetTextColor(LCD_COLOR_BLACK); 
    LCD_DisplayStringLine(LCD_LINE_6,(uint8_t*)"   IOE NOT OK      ");
    LCD_DisplayStringLine(LCD_LINE_7,(uint8_t*)"Reset the board   ");
    LCD_DisplayStringLine(LCD_LINE_8,(uint8_t*)"and try again     ");
		
		while (1) {}
	}
}

void enableInterrupt()
{
  // Enable update interrupt for TIM5
  TIM_ITConfig(TIM5, TIM_DIER_UIE, ENABLE); 
	
	// Enable timer 5 used for the interrupt
	// Done after TIM_ITConfig to be more accurate
	TIM_Cmd(TIM5, ENABLE);
	
	// Enable Interrupt (NVIC level)
	NVIC_EnableIRQ(TIM5_IRQn);  
}

void disableInterrupt()
{
  // disable the interrupt right away 
  TIM_ITConfig(TIM5, TIM_DIER_UIE, DISABLE); 
}

void startTimer2()
{
  // Enable TIM2 (used for time analysis)
	TIM_Cmd(TIM2, ENABLE);
}
	
void stopTimer2()
{
	// Stop TIM2
	TIM_Cmd(TIM2, DISABLE);
}

void startTimer3()
{
  // Enable TIM2 (used for time analysis)
	TIM_Cmd(TIM3, ENABLE);
}
	
void stopTimer3()
{
	// Stop TIM3
	TIM_Cmd(TIM3, DISABLE);
}

//Not used
void initializeLED()
{
	// initialize the LED3/4 (GPIO)
  STM_EVAL_LEDInit(LED3);
	STM_EVAL_LEDInit(LED4);
}


// Function used to create delay of 1 second
// Called from within each task
// so we make sure this routine is 
// re-entrant
void TaskWorking(uint32_t loops)
{
	uint32_t i = 0;
	for (; i < loops; i++)
	{
	}
}

/********************** DEMO APP BEGIN *************/

// This is commented out and 
// uncommented out to run a specific
// example with a specific mutex type

// for Taking mutex
#define PCP
//#define FREE_RTOS_MUTEX


#ifdef FREE_RTOS_MUTEX
#define REG_MUTEX
//#define BIN_MUTEX
#endif


// Example Running
#define EXAMPLE1
//#define EXAMPLE2
//#define EXAMPLE3
//#define EXAMPLE4
//#define EXAMPLE5
//#define EXAMPLE6


void takeMutex(uint16_t no)
{
	timesWeLocked++;
#ifdef PCP
	PCP_MutexLock(findPCP_Mutex((PCP_MutexID)no));
#endif
#ifdef FREE_RTOS_MUTEX
	xSemaphoreTake(freeRTOSMutexArray[no], portMAX_DELAY);
#endif
}

void releaseMutex(uint16_t no)
{
	timesWeUnlocked++;
#ifdef PCP
	PCP_MutexUnlock(findPCP_Mutex((PCP_MutexID)no));
#endif
#ifdef FREE_RTOS_MUTEX
	xSemaphoreGive(freeRTOSMutexArray[no]);
#endif
}


void createFreeRTOSMutexes()
{
	int i = 0;
	for (i = 0; i < 5; i++)
	{
#ifdef REG_MUTEX
	 freeRTOSMutexArray[i] = xSemaphoreCreateMutex();
#endif
#ifdef BIN_MUTEX
	 vSemaphoreCreateBinary(freeRTOSMutexArray[i]);
#endif
	}
}

/********************** EXAMPLE1 *************/

#ifdef EXAMPLE1
void createPCPMutexes()
{
#ifdef PCP
	createPCP_Mutex(t0Priority); // ID = 0
#endif
}
// task0 
void task0 (void * pvParameters)
{
	vTaskDelay(8000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
	drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t0Handle);
}
// task1 
void task1 (void * pvParameters)
{
		// delay to enable 1 to run and lock first 
	vTaskDelay(6000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t1Handle);
}
// task2
void task2 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
	vTaskDelay(4000 /portTICK_RATE_MS);
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	vTaskSuspend(t2Handle);
}
void task3 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
	vTaskDelay(2000 /portTICK_RATE_MS);
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t3Handle);
}

void task4 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
	takeMutex(0);
  TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t4Handle);
}
#endif

/********************** EXAMPLE2 *************/


#ifdef EXAMPLE2
void createPCPMutexes()
{
#ifdef PCP
	createPCP_Mutex(t0Priority); // ID = 0
	createPCP_Mutex(t1Priority); // ID = 1
#endif
}

// task0 
void task0 (void * pvParameters)
{
	vTaskDelay(5000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t0Handle);
}
// task1 
void task1 (void * pvParameters)
{
		// delay to enable 1 to run and lock first 
	vTaskDelay(3000 /portTICK_RATE_MS);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(1);
	TaskWorking(oneSecondLoops);
  releaseMutex(1);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t1Handle);
}
// task2
void task2 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
  takeMutex(1);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
  releaseMutex(1);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t2Handle);
}
void task3 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
  TaskWorking(oneSecondLoops);
  TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t3Handle);
}

void task4 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t4Handle);
}

#endif

/********************** EXAMPLE3 *************/


#ifdef EXAMPLE3
void createPCPMutexes()
{
#ifdef PCP
	createPCP_Mutex(t0Priority); // ID = 0
	createPCP_Mutex(t0Priority); // ID = 1
#endif
}
// task0 
void task0 (void * pvParameters)
{
	vTaskDelay(2000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
	takeMutex(1);
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	releaseMutex(1);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t0Handle);
}
// task1 
void task1 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	takeMutex(1);
	TaskWorking(oneSecondLoops);
	releaseMutex(0);
	releaseMutex(1);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t1Handle);
}
// task2
void task2 (void * pvParameters)
{
	vTaskSuspend(t2Handle);
}
void task3 (void * pvParameters)
{
	vTaskSuspend(t3Handle);
}

void task4 (void * pvParameters)
{
	vTaskSuspend(t4Handle);
}
#endif


/********************** EXAMPLE4 *************/


#ifdef EXAMPLE4
void createPCPMutexes()
{
#ifdef PCP
	createPCP_Mutex(t0Priority); // ID = 0
	createPCP_Mutex(t1Priority); // ID = 1
#endif
}
// task0 
void task0 (void * pvParameters)
{
	vTaskDelay(4000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
	takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t0Handle);
}
// task1 
void task1 (void * pvParameters)
{
	vTaskDelay(2000 /portTICK_RATE_MS);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	TaskWorking(oneSecondLoops);
  takeMutex(1);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	releaseMutex(1);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t1Handle);
}
// task2
void task2 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	takeMutex(1);
	TaskWorking(oneSecondLoops);
	releaseMutex(0);
	TaskWorking(oneSecondLoops);
	releaseMutex(1);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t2Handle);
}
void task3 (void * pvParameters)
{
	vTaskSuspend(t3Handle);
}

void task4 (void * pvParameters)
{
	vTaskSuspend(t4Handle);
}
#endif


/********************** EXAMPLE5 *************/


#ifdef EXAMPLE5
void createPCPMutexes()
{
#ifdef PCP
	createPCP_Mutex(t1Priority); // ID = 0
	createPCP_Mutex(t0Priority); // ID = 1
#endif
}
// task0 
void task0 (void * pvParameters)
{
	vTaskDelay(7000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
	takeMutex(1);
	TaskWorking(oneSecondLoops);
  releaseMutex(1);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t0Handle);
}
// task1 
void task1 (void * pvParameters)
{
	vTaskDelay(5000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
	releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t1Handle);
}
// task2
void task2 (void * pvParameters)
{
	vTaskDelay(4000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t2Handle);
}
void task3 (void * pvParameters)
{
	vTaskDelay(2000 /portTICK_RATE_MS);
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
  takeMutex(1);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
  takeMutex(0);
	TaskWorking(oneSecondLoops);
	releaseMutex(0);
	TaskWorking(oneSecondLoops);
	releaseMutex(1);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t3Handle);
}

void task4 (void * pvParameters)
{
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	
	TaskWorking(oneSecondLoops);
	takeMutex(0);
	TaskWorking(oneSecondLoops);
  TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	TaskWorking(oneSecondLoops);
	releaseMutex(0);
	TaskWorking(oneSecondLoops);
	
	// Mark Start / End of task with thin black stripe
  drawStartEndTask(xTaskGetCurrentTaskHandle());
	vTaskSuspend(t4Handle);
}
#endif

/********************** EXAMPLE6 *************/


#ifdef EXAMPLE6
void createPCPMutexes()
{
#ifdef PCP
	createPCP_Mutex(t0Priority); // ID = 0
	createPCP_Mutex(t1Priority); // ID = 1
	createPCP_Mutex(t2Priority); // ID = 2
	createPCP_Mutex(t3Priority); // ID = 3
	createPCP_Mutex(t4Priority); // ID = 4

#endif
}
// task0 
void task0 (void * pvParameters)
{
	for (;;){
	vTaskDelay(6000 /portTICK_RATE_MS);
	TaskWorking(oneSecondLoops);
	takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(0);
	}
}
// task1 
void task1 (void * pvParameters)
{
	for (;;){
	vTaskDelay(5000 /portTICK_RATE_MS);
	TaskWorking(oneSecondLoops);
	takeMutex(1);
	TaskWorking(oneSecondLoops);
	takeMutex(0);
	TaskWorking(oneSecondLoops);
  releaseMutex(1);
	TaskWorking(oneSecondLoops);
	releaseMutex(0);
	}
}
// task2
void task2 (void * pvParameters)
{	
	for (;;){
	vTaskDelay(3500 /portTICK_RATE_MS);
	TaskWorking(oneSecondLoops);
	takeMutex(2);
	TaskWorking(oneSecondLoops);
	takeMutex(1);
	TaskWorking(oneSecondLoops);
	releaseMutex(1);
	TaskWorking(oneSecondLoops);
  releaseMutex(2);
	}
}
void task3 (void * pvParameters)
{
	for (;;){
	vTaskDelay(1500 /portTICK_RATE_MS);
	TaskWorking(oneSecondLoops);
	takeMutex(2);
	TaskWorking(oneSecondLoops);
	takeMutex(3);
	TaskWorking(oneSecondLoops);
	releaseMutex(2);
	TaskWorking(oneSecondLoops);
  releaseMutex(3);
	}
}

void task4 (void * pvParameters)
{
		int timeLock = 0;
	int timeUnlock = 0;

	// global values that were incremented every time
	// we takeMutex / releaseMutex
	unsigned int timesWelockedLocal = 0; 
	unsigned int timesWeUnlockedLocal = 0;
	
	// Mark Start / End of task with thin black stripe
	for (;;){
	takeMutex(0);
	TaskWorking(oneSecondLoops);
	takeMutex(2);
	TaskWorking(oneSecondLoops);
	releaseMutex(0);
	TaskWorking(oneSecondLoops);
		
		
		// global values that were incremented every time
	// we takeMutex / releaseMutex
	timesWelockedLocal = timesWeLocked;
	timesWeUnlockedLocal = timesWeUnlocked;
	
	// get the time it took to lock
	timeLock = TIM_GetCounter(TIM2);

	// get the time it took to unlock
	timeUnlock = TIM_GetCounter(TIM3);
		
		// create a breakpoint here and calculate values
		// of lock/unlock time
  releaseMutex(2);
	vTaskDelay(1000 /portTICK_RATE_MS);
	}
}
#endif


// This task is used to check the value of the timers (timer 
// for locking and timer for unlocking).
// This information could have been printed on the screen, but
// unfortunatelly due to limit of size of executable (32kb), this
// was not possible, as adding sprintf increased the size of executable
// from 22kb to 33 kb (beyond limit). 
// so the debugger is used (create breakpoint before suspending this
// task) to see the value of the timers.
void task5 (void * pvParameters)
{
//	char printStatement[100];
	int timeLock = 0;
	int timeUnlock = 0;

	// global values that were incremented every time
	// we takeMutex / releaseMutex
	unsigned int timesWelockedLocal = timesWeLocked;
	unsigned int timesWeUnlockedLocal = timesWeUnlocked;
	
	// get the time it took to lock
	timeLock = TIM_GetCounter(TIM2);

	// get the time it took to unlock
	timeUnlock = TIM_GetCounter(TIM3);

	// The value of the timer in microsecond is 
	// timeLock/90 and timeUnlock/90, since timer is running at 90MHz
	
	// average lock time: = timeLock/(timesWeLocked * 90)
	// average unlock time: = timeUnlock/(timesWeUnlocked *90)
	vTaskSuspend(t5Handle);
	
}

// Main Program
int main(void)
{
	createPCPMutexes();
	createFreeRTOSMutexes();

	// initialize the LCD
  InitializeLCD();

	// Initialize the timers (TIM2 and TIM5).
	// which also helps in initialization of the interrupt
	InitializeTimers();

	initializeLED();
	
  // create t0
  xTaskCreate( task0,
              (signed char const*)t0Name,
							 t0STACK, 
							 NULL, 
							 t0Priority,
							 &t0Handle );

  // create t1
  xTaskCreate( task1,
              (signed char const*)t1Name,
							 t1STACK, 
							 NULL, 
							 t1Priority,
							 &t1Handle );
					
  // create t2				
  xTaskCreate( task2,
              (signed char const*)t2Name,
							 t2STACK, 
							 NULL, 
							 t2Priority,
							 &t2Handle );		
  // create t3				
  xTaskCreate( task3,
              (signed char const*)t3Name,
							 t3STACK, 
							 NULL, 
							 t3Priority,
							 &t3Handle );		

							  // create t2				
  xTaskCreate( task4,
              (signed char const*)t4Name,
							 t4STACK, 
							 NULL, 
							 t4Priority,
							 &t4Handle );		

							  // create t2				
  xTaskCreate( task5,
              (signed char const*)t5Name,
							 t5STACK, 
							 NULL, 
							 t5Priority,
							 &t5Handle );		

							
  // ASSERT if there is no handle returned
	configASSERT( t0Handle );
	configASSERT( t1Handle );
	configASSERT( t2Handle );
	configASSERT( t3Handle );														
	configASSERT( t4Handle );
	configASSERT( t5Handle );

	// Enable the interrupts before starting the scheduler and 
  // ignore the first interrupt (since the scheduler might not be 
  // ready at time = 0)			
  // start the work from second interrupt (, interrupt time = 100ms)	

  /* Start the FreeRTOS scheduler */
	enableInterrupt();
							
  vTaskStartScheduler();
							
	while(1) {}
}

// look at vApplicationIdleHook() in cpu_utils.c


uint16_t getColorFromPriority(unsigned portBASE_TYPE priority)
{
	static bool interlace = true;
	switch(priority)
	{
		case t0Priority:
			return LCD_COLOR_RED;
		case t1Priority:
			return LCD_COLOR_BLUE;
		case t2Priority:
			return LCD_COLOR_GREEN;
		case t3Priority:
			return LCD_COLOR_YELLOW;
		case t4Priority:
			if (interlace == true)
			{
			  interlace = false;
			  return LCD_COLOR_CYAN;
			}
			else
			{
				interlace = true;
				return LCD_COLOR_MAGENTA;
			}
		default: // IDLE priority
			return LCD_COLOR_BLACK;
	}
}

/*
  * @param  Xpos: specifies the X position, can be a value from 0 to 240.
  * @param  Ypos: specifies the Y position, can be a value from 0 to 320.
  * @param  Height: display rectangle height, can be a value from 0 to 320.
  * @param  Width: display rectangle width, can be a value from 0 to 240.
  * @retval None
  LCD_DrawLine(Xpos, Ypos, Width, LCD_DIR_HORIZONTAL);

  */
	

// The APIs provided by STI only require that 
// we define this function and it will be called
// when we enable the interrupt for TIM5
void TIM5_IRQHandler(void)
{
	xTaskHandle taskHandle = xTaskGetCurrentTaskHandle();
	// NOTE:
	// This configuration of the interrupt handler was found to be 
	// more efficient (clock count) than 
	// if(TIM5->SR & TIM_SR_UIF)  // if UIF flag is set
  // {
	// 	count++;                 // increment the IRQ count
  // 	TIM5->SR &= ~TIM_SR_UIF; // clear UIF flag
	// }
	// I think the reason for that, is because the variables TIM5->SR
	// and TIM_SR_UIF are still loaded in the CPU registers (from the previous
	// operation, which is ANDing it with TIM_SR_UIF and checking
	// the result.). The calcuation TIM5->SR &= ~TIM_SR_UIF becomes faster.
	// The efficient (below) IRQ routine consumes ~ 62 clocks
	// while the other one (above in comments) consumes 
	// 87 clock cycles !
	if(TIM5->SR & TIM_SR_UIF)  // if UIF flag is set
  {
  	TIM5->SR &= ~TIM_SR_UIF; // clear UIF flag
				
		if (taskHandle == t0Handle)
	  {
			LCD_SetTextColor(getColorFromPriority(getTaskPriority(t0Handle)));
      LCD_DrawLine(task0XPos, lineNo--, 20, LCD_DIR_HORIZONTAL);
		}
		else if (taskHandle == t1Handle)
		{
			LCD_SetTextColor(getColorFromPriority(getTaskPriority(t1Handle)));
      LCD_DrawLine(task1XPos, lineNo--, 20, LCD_DIR_HORIZONTAL);
		}
		else if (taskHandle == t2Handle)
		{
			LCD_SetTextColor(getColorFromPriority(getTaskPriority(t2Handle)));
      LCD_DrawLine(task2XPos, lineNo--, 20, LCD_DIR_HORIZONTAL);
		}
		else if (taskHandle == t3Handle)
		{
			LCD_SetTextColor(getColorFromPriority(getTaskPriority(t3Handle)));
      LCD_DrawLine(task3XPos, lineNo--, 20, LCD_DIR_HORIZONTAL);
		}		
		else if (taskHandle == t4Handle)
		{
			LCD_SetTextColor(getColorFromPriority(getTaskPriority(t4Handle)));
      LCD_DrawLine(task4XPos, lineNo--, 20, LCD_DIR_HORIZONTAL);
		}		
		else
		{
			LCD_SetTextColor(getColorFromPriority(0));
      LCD_DrawLine(idleXPos, lineNo--, 20, LCD_DIR_HORIZONTAL);
		}
	}
}

// defines and functions required for compile. 
// handles an assertion in the libraries / added files
#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @brief  Error callback function
  * @param  None
  * @retval None
  */
void vApplicationMallocFailedHook( void )
{
  while (1)
  {
	}
}
