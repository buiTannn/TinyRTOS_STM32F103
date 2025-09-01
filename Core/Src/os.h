// os.h - Header file for STM32F103 RTOS
#ifndef OS_H
#define OS_H

#include "stm32f1xx.h"
#include <stdint.h>

// Configuration defines
#define MAXTHREADS 8
#define STACKSIZE 256

// TCB Status enumeration
typedef enum {
    TCB_FREE = 0,
    TCB_ACTIVE,
    TCB_SLEEPING
} TCBStatus_t;

// Thread Control Block structure
typedef struct TCB {
    uint32_t *sp;           // Stack pointer
    struct TCB *next;       // Pointer to next TCB in linked list
    TCBStatus_t status;     // Thread status
    uint8_t priority;       // Priority (0 = highest, 255 = lowest)
    uint32_t sleep;         // Sleep counter in ticks
    const char *name;       // Thread name
} TCB_t;

// Global variables
extern TCB_t tcbs[MAXTHREADS];
extern uint32_t stacks[MAXTHREADS][STACKSIZE];
extern TCB_t *RunPt;
extern int ActiveThreads;
extern volatile uint8_t OS_Running;
extern uint16_t stack_max_used[MAXTHREADS];
extern uint8_t stack_overflow_count[MAXTHREADS];

// OS Functions
void OS_Init(void);
int OS_AddThread(void (*task)(void), uint8_t priority, const char *name);
void OS_Sleep(uint32_t ms);
void OS_Launch(uint32_t tick_hz);
void OS_Scheduler(void);
void OS_TickHandler(void);
void CheckStackOverflow(int i);
void IdleTask(void);

// Assembly functions
void StartOS(void);

#endif // OS_H