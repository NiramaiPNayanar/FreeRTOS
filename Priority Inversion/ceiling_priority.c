#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <stdint.h>

#ifdef WIN32_LEAN_AND_MEAN
#include "winsock2.h"
#else
#include <winsock.h>
#endif

#include <intrin.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "trcRecorder.h"


#define mainREGION_1_SIZE               82010
#define mainREGION_2_SIZE               239050
#define mainREGION_3_SIZE               168070
#define mainNO_KEY_PRESS_VALUE          -1
#define mainOUTPUT_TRACE_KEY            't'
#define mainINTERRUPT_NUMBER_KEYBOARD   3
#define mainTRACE_FILE_NAME             "Trace.dump"


static void prvInitialiseHeap(void);
static void prvSaveTraceFile(void);
static uint32_t prvKeyboardInterruptHandler(void);
static int32_t WINAPI prvWindowsKeyboardInputThread(void* pvParam);


StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
static HANDLE xWindowsKeyboardInputThreadHandle = NULL;
static int xKeyPressed = mainNO_KEY_PRESS_VALUE;


#define PRIO_SAFETY   3 
#define PRIO_MOTOR    2 
#define PRIO_DIAG     1 
#define DEMO_STACK_SZ 1000


static SemaphoreHandle_t xMotorLock = NULL;


static UBaseType_t raise_to_ceiling_and_take_lock(void)
{
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    UBaseType_t prev = uxTaskPriorityGet(cur);
    
    vTaskPrioritySet(cur, PRIO_SAFETY);
    
    xSemaphoreTake(xMotorLock, portMAX_DELAY);
    return prev;
}

static void give_lock_and_restore(UBaseType_t prev)
{
    xSemaphoreGive(xMotorLock);
    
    vTaskPrioritySet(NULL, prev);
}


static void MotorControlTask(void* pvParameters);
static void DiagnosticsTask(void* pvParameters);
