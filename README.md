# STM32F103 Real-Time Operating System (RTOS)

A lightweight, preemptive Real-Time Operating System implementation for STM32F103 microcontroller using ARM Cortex-M3 architecture.

## Features

- **Preemptive Multitasking**: Higher priority tasks can interrupt lower priority tasks
- **Priority-based Scheduling**: Tasks are scheduled based on priority levels (0 = highest, 255 = lowest)
- **Round-Robin Scheduling**: Tasks with the same priority share CPU time equally
- **Sleep Function**: Tasks can sleep for specified number of ticks
- **Idle Task**: Always-ready task that runs when no other tasks are active

## System Components

### Core Files
- `os.c` - Main RTOS implementation
- `os.h` - Header file with data structures and function prototypes  
- `osasm.s` - ARM assembly code for context switching and startup

### Key Functions
- `OS_Init()` - Initialize the operating system
- `OS_AddThread()` - Add a new task to the system
- `OS_Sleep()` - Put current task to sleep for specified ticks
- `OS_Launch()` - Start the operating system with specified tick frequency
- `OS_Scheduler()` - Priority-based task scheduler

## Installation Guide

### Step 1: Create STM32CubeMX Project

<p align="center"> <img src="https://github.com/user-attachments/assets/2ded64d3-78bd-4dd8-b14b-1ca159a410d6" width="548" height="333" alt="Image 1" /> </p> <p align="center"><i>Select Clock → Crystal/Ceramic Resonator in RCC</i></p> 
<p align="center"> <img src="https://github.com/user-attachments/assets/86807cee-689e-4e8b-8156-a0df750b99be" width="780" alt="Image 2" /> </p> <p align="center"><i>The TIM1 configuration used for HAL</i></p>


### Step 2: Add RTOS Files
1. Copy the following files to your project's `Src` folder (where `main.c` is located):
   - `os.c`
   - `os.h` 
   - `osasm.s`

2. Make sure these files are included in your project build

### Step 3: Include Headers
1. Add `#include "stm32f1xx.h"` to your `main.c` file (if not already present)
2. Add `#include "os.h"` to your `stm32f1xx_it.c` file

### Step 4: Modify Interrupt Handlers
1. **Delete** the `PendSV_Handler` function from `stm32f1xx_it.c` (our RTOS provides its own)
2. **Add** `OS_TickHandler();` to the `SysTick_Handler` function in `stm32f1xx_it.c`:

```c
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  OS_TickHandler();  // Add this line
  /* USER CODE END SysTick_IRQn 1 */
}
```

### Step 5: Create Tasks and Launch RTOS

Replace your `main.c` with the following example:

```c
#include "main.h"
#include "os.h"

// Task variables
int m = 0;
int k = 0;

// Task 1 - High priority task
void Task1(void) {
    while(1) {
        m++;
        OS_Sleep(1000);  // Sleep for 1000 ticks (1 second at 1kHz)
    }
}

// Task 2 - High priority task  
void Task2(void) {
    while(1) {
        k++;
        OS_Sleep(2000);  // Sleep for 2000 ticks (2 seconds at 1kHz)
    }
}

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  /* USER CODE BEGIN 2 */
  
  // Initialize RTOS
  OS_Init();
  
  // Add tasks to the system
  // int OS_AddThread(void (*task)(void), uint8_t priority, const char *name);
  OS_AddThread(Task1, 0, "Task1");  // Priority 0 (highest)
  OS_AddThread(Task2, 0, "Task2");  // Priority 0 (highest)
  
  // Launch RTOS with 1000Hz tick rate (1ms per tick)
  OS_Launch(1000);

```


## Debugging
- m increases every 1 second and m increases every 2 seconds.
  
<img width="1192" height="871" alt="image" src="https://github.com/user-attachments/assets/fc2a2ea2-762c-40a2-bd5d-b19e637d208f" />

- Use debugger to monitor task counters (`m`, `k` variables)
- Check `RunPt->name` to see which task is currently running
- Monitor `stack_overflow_count[]` array for stack overflow detection
- Set breakpoints in tasks to verify execution


## Troubleshooting

**Problem**: Tasks not switching properly
- **Solution**: Ensure `OS_TickHandler()` is called in `SysTick_Handler()`

**Problem**: System hangs at startup  
- **Solution**: Check that `PendSV_Handler` is removed from `stm32f1xx_it.c`

**Problem**: Stack overflow errors
- **Solution**: Increase `STACKSIZE` or reduce local variable usage in tasks
