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

SemaphoreHandle_t xResourceLock;

void LoggerTask(void* pvParameters);
void ComputationTask(void* pvParameters);
void TelemetryTask(void* pvParameters);

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

    xResourceLock = xSemaphoreCreateMutex();

    xSemaphoreGive(xResourceLock);

    xTaskCreate(LoggerTask, "Logger", 1000, NULL, 1, NULL);
    xTaskCreate(ComputationTask, "Compute", 1000, NULL, 2, NULL);
    xTaskCreate(TelemetryTask, "Telemetry", 1000, NULL, 3, NULL);

    vTaskStartScheduler();

    for (;;);
    return 0;
}

void LoggerTask(void* pvParameters)
{
    for (;;)
    {
        if (xSemaphoreTake(xResourceLock, portMAX_DELAY))
        {
            printf("[Logger][Low] Writing to shared resource...\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
            printf("[Logger][Low] Done writing.\n");
            xSemaphoreGive(xResourceLock);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void ComputationTask(void* pvParameters)
{
    for (;;)
    {
        printf("[Computation][Medium] Crunching numbers...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TelemetryTask(void* pvParameters)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(7000));
        printf("[Telemetry][High] Needs urgent access!\n");

        if (xSemaphoreTake(xResourceLock, portMAX_DELAY))
        {
            printf("[Telemetry][High] Accessing shared resource urgently!\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            printf("[Telemetry][High] Done.\n");
            xSemaphoreGive(xResourceLock);
        }
    }
}

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

static uint32_t ulEntryTime = 0;
void vTraceTimerReset(void) { ulEntryTime = xTaskGetTickCount(); }
uint32_t uiTraceTimerGetFrequency(void) { return configTICK_RATE_HZ; }
uint32_t uiTraceTimerGetValue(void) { return (xTaskGetTickCount() - ulEntryTime); }
