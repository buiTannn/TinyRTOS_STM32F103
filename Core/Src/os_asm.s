        AREA    OSASM, CODE, READONLY
        PRESERVE8
        THUMB
        
        EXPORT  OS_DisableInterrupts
        EXPORT  OS_EnableInterrupts
        EXPORT  StartOS
        EXPORT  PendSV_Handler
        
        EXTERN  RunPt
        EXTERN  OS_Scheduler

; Simple interrupt control functions
OS_DisableInterrupts
        CPSID   I
        BX      LR

OS_EnableInterrupts
        CPSIE   I
        BX      LR

; StartOS - Initialize first thread (Keil syntax, adapted from F3)
StartOS
        CPSID   I                       ; disable interrupts
        LDR     R0, =RunPt              ; R0 = &RunPt;  // TCB_t** R0 = &RunPt
        LDR     R1, [R0]                ; R1 = *R0;     // TCB_t*  R1 = RunPt
        
        ; Validate RunPt is not NULL
        CMP     R1, #0
        BEQ     StartOS_Error
        
        LDR     SP, [R1]                ; SP = *R1;     // uint32_t SP = *(RunPt->sp)
        
        ; Validate SP range for STM32F103 (20KB RAM: 0x20000000-0x20005000)
        LDR     R2, =0x20000200         ; Min valid SP (after globals)
        CMP     SP, R2
        BLO     StartOS_Error
        
        LDR     R2, =0x20004F00         ; Max valid SP  
        CMP     SP, R2
        BHS     StartOS_Error
        
        ; Check 8-byte alignment
        MOV     R3, SP
		TST     R3, #0x07

        BNE     StartOS_Error
        
        ; Now we switched to the thread's stack, restore context
        ; ULTRA SAFE: Manual stack operations to avoid any UNPREDICTABLE
        LDR     R4, [SP]                ; Load R4 from stack
        ADD     SP, SP, #4              ; Move SP up
        LDR     R5, [SP]                ; Load R5 from stack
        ADD     SP, SP, #4              ; Move SP up
        LDR     R6, [SP]                ; Load R6 from stack
        ADD     SP, SP, #4              ; Move SP up
        LDR     R7, [SP]                ; Load R7 from stack
        ADD     SP, SP, #4              ; Move SP up
        LDR     R8, [SP]                ; Load R8 from stack
        ADD     SP, SP, #4              ; Move SP up
        LDR     R9, [SP]                ; Load R9 from stack
        ADD     SP, SP, #4              ; Move SP up
        LDR     R10, [SP]               ; Load R10 from stack
        ADD     SP, SP, #4              ; Move SP up
        LDR     R11, [SP]               ; Load R11 from stack
        ADD     SP, SP, #4              ; Move SP up
        POP     {R0}                    ; pop R0
        POP     {R1}                    ; pop R1
        POP     {R2}                    ; pop R2
        POP     {R3}                    ; pop R3
        POP     {R12}                   ; pop R12
        POP     {LR}                    ; discard saved LR
        POP     {LR}                    ; pop PC to the link register (task start)
        POP     {R1}                    ; discard PSR
        CPSIE   I                       ; enable interrupts
        BX      LR                      ; start first thread

StartOS_Error
        LDR     R0, =0xBADC0DE1         ; Error signature
        B       StartOS_Error           ; Hang for debugging

; PendSV_Handler - Context switch (Keil syntax, adapted from F3)
PendSV_Handler
        ; Hardware automatically saves R0-R3, R12, LR, PC, PSR to stack
        
        ; Quick safety check
        LDR     R0, =RunPt              ; R0 = &RunPt
        LDR     R1, [R0]                ; R1 = RunPt
        CMP     R1, #0
        BEQ     PendSV_Exit
        
        CPSID   I                       ; prevent interrupt during context-switch
        
        ; SAFE: Use individual PUSH operations to avoid UNPREDICTABLE
        PUSH    {R4}                    ; save R4
        PUSH    {R5}                    ; save R5
        PUSH    {R6}                    ; save R6
        PUSH    {R7}                    ; save R7
        PUSH    {R8}                    ; save R8
        PUSH    {R9}                    ; save R9
        PUSH    {R10}                   ; save R10
        PUSH    {R11}                   ; save R11
        
        ; Validate current stack
        MOV     R2, SP
        LDR     R3, =0x20000200         ; Min stack address
        CMP     R2, R3
        BLO     PendSV_Error            ; Stack underflow
        
        LDR     R3, =0x20004F00         ; Max stack address  
        CMP     R2, R3
        BHS     PendSV_Error            ; Stack overflow
        
        STR     SP, [R1]                ; Save current SP: *(RunPt->sp) = SP
        
        ; Call scheduler - preserve R0 and LR for function call
        PUSH    {R0}                    ; push R0
        PUSH    {LR}                    ; push LR (8-byte aligned)
        BL      OS_Scheduler            ; call OS_Scheduler, RunPt is updated
        POP     {LR}                    ; restore LR
        POP     {R0}                    ; restore R0
        
        ; Load new thread context
        LDR     R1, [R0]                ; R1 = RunPt (new thread)
        CMP     R1, #0                  ; Check for NULL
        BEQ     PendSV_Error
        
        LDR     SP, [R1]                ; SP = *(RunPt->sp) - switch to new thread's stack
        
        ; Validate new SP
        LDR     R2, =0x20000200
        CMP     SP, R2
        BLO     PendSV_Error            ; Invalid SP
        
        LDR     R2, =0x20004F00
        CMP     SP, R2  
        BHS     PendSV_Error            ; Invalid SP
        
        ; Check alignment
        MOV     R3, SP
		TST     R3, #0x07

        BNE     PendSV_Error            ; Not aligned
        
        ; SAFE: Use individual POP operations (reverse order)
        POP     {R11}                   ; restore R11
        POP     {R10}                   ; restore R10
        POP     {R9}                    ; restore R9
        POP     {R8}                    ; restore R8
        POP     {R7}                    ; restore R7
        POP     {R6}                    ; restore R6
        POP     {R5}                    ; restore R5
        POP     {R4}                    ; restore R4
        
        CPSIE   I                       ; tasks run with interrupts enabled
        BX      LR                      ; return - hardware restores R0-R3,R12,LR,PC,PSR

PendSV_Exit
        BX      LR

PendSV_Error
        ; Stack corruption or invalid state detected
        LDR     R0, =0xDEADBEEF         ; Error signature
        MOV     R1, SP                  ; Current SP for debugging
        LDR     R2, =RunPt
        LDR     R2, [R2]                ; RunPt value
        B       PendSV_Error            ; Infinite loop - set breakpoint here

        END