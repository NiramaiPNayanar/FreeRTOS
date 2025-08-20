
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
#include "trcRecorder.h"

 /*-----------------------------------------------------------*/
#define mainREGION_1_SIZE                     82010
#define mainREGION_2_SIZE                     239050
#define mainREGION_3_SIZE                     168070
#define mainNO_KEY_PRESS_VALUE                -1
#define mainOUTPUT_TRACE_KEY                  't'
#define mainINTERRUPT_NUMBER_KEYBOARD         3
#define mainTRACE_FILE_NAME                   "Trace.dump"

/*-----------------------------------------------------------*/
static void prvInitialiseHeap(void);
static void prvSaveTraceFile(void);
static uint32_t prvKeyboardInterruptHandler(void);
static int32_t WINAPI prvWindowsKeyboardInputThread(void* pvParam);

/*-----------------------------------------------------------*/
/* Memory for static allocation */
StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
static HANDLE xWindowsKeyboardInputThreadHandle = NULL;
static int xKeyPressed = mainNO_KEY_PRESS_VALUE;

/*-----------------------------------------------------------*/
/* === EDF Simulation Structures === */
#define EDF_TASK_COUNT 4
#define EDF_TASK_RUNS 5

typedef struct {
    TaskHandle_t handle;
    TickType_t period;
    TickType_t next_deadline;
    const char* name;
} EDFTaskInfo;

static EDFTaskInfo edfTasks[EDF_TASK_COUNT];
static const UBaseType_t edfPriority[EDF_TASK_COUNT] = { 5, 4, 3, 2 }; // Highest to lowest

/*-----------------------------------------------------------*/
void vDroneMission(void* pvParameters)
{
    EDFTaskInfo* info = (EDFTaskInfo*)pvParameters;
    int runCount = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (; runCount < EDF_TASK_RUNS; ++runCount)
    {
        printf("%s flying (deadline: %lu, mission %d)\n", info->name, (unsigned long)info->next_deadline, runCount + 1);
        info->next_deadline = xLastWakeTime + info->period;
        vTaskDelayUntil(&xLastWakeTime, info->period);
    }
    printf("%s returned to base after %d missions\n", info->name, EDF_TASK_RUNS);
    vTaskDelete(NULL);
}

/*-----------------------------------------------------------*/
/* EDF Scheduler Task: periodically updates priorities */
void vEDFSchedulerTask(void* pvParameters)
{
    for (;;)
    {
        int idx[EDF_TASK_COUNT] = { 0, 1, 2, 3 };
        for (int i = 0; i < EDF_TASK_COUNT - 1; ++i) {
            for (int j = i + 1; j < EDF_TASK_COUNT; ++j) {
                if (edfTasks[idx[j]].handle != NULL &&
                    (edfTasks[idx[i]].handle == NULL || edfTasks[idx[j]].next_deadline < edfTasks[idx[i]].next_deadline)) {
                    int tmp = idx[i];
                    idx[i] = idx[j];
                    idx[j] = tmp;
                }
            }
        }
        for (int i = 0; i < EDF_TASK_COUNT; ++i) {
            if (edfTasks[idx[i]].handle != NULL) {
                vTaskPrioritySet(edfTasks[idx[i]].handle, edfPriority[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/*-----------------------------------------------------------*/
/* === MAIN FUNCTION === */
int main(void)
{
    prvInitialiseHeap();

    configASSERT(xTraceInitialize() == TRC_SUCCESS);
    printf(
        "Trace started.\r\n"
        "Trace file: %s (press '%c' to dump)\r\n",
        mainTRACE_FILE_NAME, mainOUTPUT_TRACE_KEY);
    configASSERT(xTraceEnable(TRC_START) == TRC_SUCCESS);

    vPortSetInterruptHandler(mainINTERRUPT_NUMBER_KEYBOARD, prvKeyboardInterruptHandler);
    xWindowsKeyboardInputThreadHandle = CreateThread(NULL, 0, prvWindowsKeyboardInputThread, NULL, 0, NULL);
    SetThreadAffinityMask(xWindowsKeyboardInputThreadHandle, ~0x01u);

    /* === Create EDF Drone Missions === */
    edfTasks[0].period = pdMS_TO_TICKS(2000);
    edfTasks[0].next_deadline = xTaskGetTickCount() + edfTasks[0].period;
    edfTasks[0].name = "Drone1";
    xTaskCreate(vDroneMission, "Drone1", 1000, &edfTasks[0], edfPriority[3], &edfTasks[0].handle);

    edfTasks[1].period = pdMS_TO_TICKS(500);
    edfTasks[1].next_deadline = xTaskGetTickCount() + edfTasks[1].period;
    edfTasks[1].name = "Drone2";
    xTaskCreate(vDroneMission, "Drone2", 1000, &edfTasks[1], edfPriority[3], &edfTasks[1].handle);

    edfTasks[2].period = pdMS_TO_TICKS(750);
    edfTasks[2].next_deadline = xTaskGetTickCount() + edfTasks[2].period;
    edfTasks[2].name = "Drone3";
    xTaskCreate(vDroneMission, "Drone3", 1000, &edfTasks[2], edfPriority[3], &edfTasks[2].handle);

    edfTasks[3].period = pdMS_TO_TICKS(1200);
    edfTasks[3].next_deadline = xTaskGetTickCount() + edfTasks[3].period;
    edfTasks[3].name = "Drone4";
    xTaskCreate(vDroneMission, "Drone4", 1000, &edfTasks[3], edfPriority[3], &edfTasks[3].handle);

    xTaskCreate(vEDFSchedulerTask, "DroneScheduler", 1000, NULL, edfPriority[0] + 1, NULL);

    vTaskStartScheduler();

    for (;;);
    return 0;
}

/*-----------------------------------------------------------*/
/* === Hook Functions (leave as is) === */
void vApplicationMallocFailedHook(void) { vAssertCalled(__LINE__, __FILE__); }
void vApplicationIdleHook(void) {}
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char* pcTaskName)
{
    (void)pcTaskName; (void)pxTask; vAssertCalled(__LINE__, __FILE__);
}
void vApplicationTickHook(void) {}
void vApplicationDaemonTaskStartupHook(void) {}

void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
    StackType_t** ppxIdleTaskStackBuffer,
    configSTACK_DEPTH_TYPE* puxIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *puxIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
    StackType_t** ppxTimerTaskStackBuffer,
    uint32_t* pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

/*-----------------------------------------------------------*/
/* === Internal Functions (keep original) === */
void vAssertCalled(unsigned long ulLine, const char* const pcFileName)
{
    volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;
    taskENTER_CRITICAL();
    {
        printf("ASSERT! Line %ld, file %s, GetLastError() %ld\r\n", ulLine, pcFileName, GetLastError());
        (void)xTraceDisable();
        prvSaveTraceFile();
        __debugbreak();
        while (ulSetToNonZeroInDebuggerToContinue == 0) { __nop(); }
        (void)xTraceEnable(TRC_START);
    }
    taskEXIT_CRITICAL();
}

static void prvSaveTraceFile(void)
{
    FILE* pxOutputFile;
    fopen_s(&pxOutputFile, mainTRACE_FILE_NAME, "wb");
    if (pxOutputFile != NULL)
    {
        fwrite(RecorderDataPtr, sizeof(RecorderDataType), 1, pxOutputFile);
        fclose(pxOutputFile);
        printf("Trace output saved to %s\r\n", mainTRACE_FILE_NAME);
    }
}

static void prvInitialiseHeap(void)
{
    static uint8_t ucHeap[configTOTAL_HEAP_SIZE];
    volatile uint32_t ulAdditionalOffset = 19;
    const HeapRegion_t xHeapRegions[] =
    {
        { ucHeap + 1,                                          mainREGION_1_SIZE },
        { ucHeap + 15 + mainREGION_1_SIZE,                     mainREGION_2_SIZE },
        { ucHeap + 19 + mainREGION_1_SIZE + mainREGION_2_SIZE, mainREGION_3_SIZE },
        { NULL, 0 }
    };
    configASSERT((ulAdditionalOffset + mainREGION_1_SIZE + mainREGION_2_SIZE + mainREGION_3_SIZE) < configTOTAL_HEAP_SIZE);
    (void)ulAdditionalOffset;
    vPortDefineHeapRegions(xHeapRegions);
}

static uint32_t prvKeyboardInterruptHandler(void)
{
    switch (xKeyPressed)
    {
    case mainNO_KEY_PRESS_VALUE:
        break;
    case mainOUTPUT_TRACE_KEY:
        portENTER_CRITICAL();
        {
            (void)xTraceDisable();
            prvSaveTraceFile();
            (void)xTraceEnable(TRC_START);
        }
        portEXIT_CRITICAL();
        break;
    default:
        break;
    }
    return pdFALSE;
}

static int32_t WINAPI prvWindowsKeyboardInputThread(void* pvParam)
{
    (void)pvParam;
    for (;;)
    {
        xKeyPressed = _getch();
        vPortGenerateSimulatedInterrupt(mainINTERRUPT_NUMBER_KEYBOARD);
    }
    return -1;
}

/* Trace recorder timing helpers */
static uint32_t ulEntryTime = 0;
void vTraceTimerReset(void) { ulEntryTime = xTaskGetTickCount(); }
uint32_t uiTraceTimerGetFrequency(void) { return configTICK_RATE_HZ; }
uint32_t uiTraceTimerGetValue(void) { return (xTaskGetTickCount() - ulEntryTime); }
