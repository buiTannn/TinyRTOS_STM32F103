#include "os.h"
#include "stm32f1xx_it.h"


// Global variables
TCB_t tcbs[MAXTHREADS];
uint32_t stacks[MAXTHREADS][STACKSIZE];  
TCB_t *RunPt = NULL;
int ActiveThreads = 0;
volatile uint8_t OS_Running = 0;

// Stack monitoring
uint16_t stack_max_used[MAXTHREADS];
uint8_t stack_overflow_count[MAXTHREADS];

#define STACK_CANARY 0xDEADBEEF
#define STACK_FILL_PATTERN 0xA5A5A5A5

    /* https://developer.arm.com/docs/100235/0002/the-cortexm33-processor/exception-model/exception-entry-and-return
     *
     *   ARM Stack:  *
     * | {aligner} | * high addresses
     * |   xPSR    | *
     * |    PC     | *      ||
     * |  LR (R14) | *      || Stack
     * |    R12    | *      || grows
     * |    R3     | *      || down!
     * |    R2     | *      ||
     * |    R1     | *      \/
     * |    R0     | *
     * |  R11-R4   | * low addresses
     * | {storage} | *
     */
		 
static void SetInitialStack(int i, void (*task)(void)) {
    if (i >= MAXTHREADS) return;
    for (int j = 0; j < STACKSIZE; j++) {
        stacks[i][j] = STACK_FILL_PATTERN;
    }
    
    // Canary at bottom
    stacks[i][0] = STACK_CANARY;
    
    // Stack grows downward from top
    uint32_t *sp = &stacks[i][STACKSIZE];
    sp = (uint32_t*)((uint32_t)sp & ~0x7UL);
    *(--sp) = (1U << 24);           // xPSR (Thumb bit set)
    *(--sp) = (uint32_t)task;       // PC (return address)
    *(--sp) = 0xFFFFFFFD;           // LR (EXC_RETURN - Thread mode, PSP)
    *(--sp) = 0x12121212;           // R12
    *(--sp) = 0x03030303;           // R3
    *(--sp) = 0x02020202;           // R2
    *(--sp) = 0x01010101;           // R1
    *(--sp) = 0x00000000;           // R0

    // Software frame (saved by our context switch)
    *(--sp) = 0x11111111;           // R11
    *(--sp) = 0x10101010;           // R10
    *(--sp) = 0x09090909;           // R9
    *(--sp) = 0x08080808;           // R8
    *(--sp) = 0x07070707;           // R7
    *(--sp) = 0x06060606;           // R6
    *(--sp) = 0x05050505;           // R5
    *(--sp) = 0x04040404;           // R4
    
    tcbs[i].sp = sp;
}

// Enhanced stack checking
void CheckStackOverflow(int i) {
    if (i >= MAXTHREADS || tcbs[i].status == TCB_FREE) return;
    
    if (stacks[i][0] != STACK_CANARY) {
        stack_overflow_count[i]++;
        tcbs[i].status = TCB_FREE;
        return;
    }
}
//IDLE TASK
void IdleTask(void) {
    while(1) {
        //Do Nothing
    }
}

void OS_Init(void) {
    // Critical: Disable interrupts during init
    __disable_irq();
    
    // Initialize all TCBs
    for (int i = 0; i < MAXTHREADS; i++) {
        tcbs[i].status = TCB_FREE;
        tcbs[i].sp = NULL;
        tcbs[i].next = NULL;
        tcbs[i].priority = 0;
        tcbs[i].sleep = 0;
        tcbs[i].name = NULL;
        stack_max_used[i] = 0;
        stack_overflow_count[i] = 0;
    }

    // Setup Idle task at index 0 (always exists)
    SetInitialStack(0, IdleTask);
    tcbs[0].status = TCB_ACTIVE;
    tcbs[0].priority = 255;  // Lowest priority
    tcbs[0].name = "Idle";
    tcbs[0].next = &tcbs[0]; 
    tcbs[0].sleep = 0;

    RunPt = &tcbs[0];
    ActiveThreads = 1;
    OS_Running = 0; 
    
    __enable_irq();
}

int OS_AddThread(void (*task)(void), uint8_t priority, const char *name) {
    if (task == NULL) return 0;
    
    __disable_irq();
    
    for (int i = 1; i < MAXTHREADS; i++) {
        if (tcbs[i].status == TCB_FREE) {
            // Setup new TCB
            tcbs[i].status = TCB_ACTIVE;
            tcbs[i].priority = priority;
            tcbs[i].sleep = 0;
            tcbs[i].name = name;
            
            SetInitialStack(i, task);
            
            if (ActiveThreads == 1) {
                tcbs[i].next = &tcbs[0];    
                tcbs[0].next = &tcbs[i];    
            } else {
                tcbs[i].next = tcbs[0].next;  
                tcbs[0].next = &tcbs[i];      
            }
            
            ActiveThreads++;
            
            __enable_irq();
            return 1; 
        }
    }
    
    __enable_irq();
    return 0;
}

void OS_Sleep(uint32_t ms) {
    if (RunPt == NULL || !OS_Running) return;
    
    __disable_irq();
    RunPt->sleep = ms;
    RunPt->status = (ms > 0) ? TCB_SLEEPING : TCB_ACTIVE;
    __enable_irq();
    
    // Trigger context switch
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

//SCHEDULER
void OS_Scheduler(void) {
    if (!OS_Running || RunPt == NULL) return;

    TCB_t* best = NULL;
    uint8_t best_priority = 255;  // Start with worst priority
    
    // Search all threads for the highest priority active thread
    for (int i = 0; i < MAXTHREADS; i++) {
        if (tcbs[i].status == TCB_ACTIVE && tcbs[i].sleep == 0) {
            // Lower number = higher priority
            if (tcbs[i].priority < best_priority) {
                best = &tcbs[i];
                best_priority = tcbs[i].priority;
            }
            // If same priority, use round-robin
            else if (tcbs[i].priority == best_priority && &tcbs[i] == RunPt) {
                best = &tcbs[i];
            }
        }
    }

    // Switch to the best thread found
    if (best != NULL) {
        RunPt = best;
    } else {
        RunPt = &tcbs[0];
    }
}

void OS_Launch(uint32_t tick_hz) {
    if (ActiveThreads == 0) return;
    
    // Setup SysTick for periodic interrupts
    SysTick->LOAD = (SystemCoreClock / tick_hz) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | 
                    SysTick_CTRL_TICKINT_Msk | 
                    SysTick_CTRL_ENABLE_Msk;
    
    // Set PendSV to lowest priority
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    
    OS_Running = 1;
    
    // Run scheduler once to select best thread before starting
    OS_Scheduler();
    
    // Start the selected thread
    StartOS();
    
    // Should never reach here
    while(1) {}
}

void OS_TickHandler(void) {
    if (!OS_Running) return;
    
    static uint8_t tick_counter = 0;
    int needSwitch = 0;
    
    // Process sleep timers
    for (int i = 0; i < MAXTHREADS; i++) {
        if (tcbs[i].status == TCB_SLEEPING && tcbs[i].sleep > 0) {
            tcbs[i].sleep--;
            if (tcbs[i].sleep == 0) {
                tcbs[i].status = TCB_ACTIVE;
                needSwitch = 1;
            }
        }
    }
    
    if (++tick_counter >= 100) {
        tick_counter = 0;
        for (int i = 0; i < MAXTHREADS; i++) {
            if (tcbs[i].status != TCB_FREE) {
                CheckStackOverflow(i);
            }
        }
    }
    // This ensures higher priority tasks can preempt lower priority ones
    needSwitch = 1;

    // Trigger context switch
    if (needSwitch) {
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }
}
