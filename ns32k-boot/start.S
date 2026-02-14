
/* NS32K first stage bootloader and monitor startup file
 *
 * This code provides a minimal startup environment for the NS32K-104 bootloader
 *
 * The code:-
 *
 *  - Initialises the PSR (sets the S bit to select the Supervisor stack)
 *
 *  - Relocates itself from 0x000000 to 0xF00000 by
 *    Jumping to 0xF0000A
 *    Setting UART0 OUT1 active (LOW) which acts as REMAP signal to the memeory address decoder PLD
 *  
 *  - Configures UART0 for 38400,n,8,1
 * 
 *  - Writes '\r' to UART0
 *  - Writes 'L' to UART0
 *  - This serves as an initial HW check and does not depend on working RAM
 * 
 *  - Performs a simple RAM presence test at STACK_TOP  
 *
 *  - If sucessful Writes '3' the '2' to UART0
 *
 *  - Sets the Supervisor stack to STACK_TOP
 *
 *  - Jumps to C start 
 *
 *  - If C ever returns, jump back to start
 *
 *  - This code does not setup the Module or Interrupt tables
 *  - These are left to be setup by the application
 *
 * Copyright Mike Cornelius 2026.
 *
 */

#define UART0_DLL 0xFE0000
#define UART0_THR 0xFE0000
#define UART0_IER 0xFE0004
#define UART0_LCR 0xFE000C
#define UART0_MCR 0xFE0010
#define UART0_LSR 0xFE0014

#define STACK_TOP 0x01FFFC


    .text
    .globl start
    .extern _cstart

start: 
    bicpsrw 9                   // Set PSR S bit - Select Supervisor stack
    jump    @0xF0000A           // At reset the PC is set to 0, relocate to 0xF0000A 
    movb    0x0F, @UART0_MCR    // Set UART0 MCR = DTR | RTS | OUT1 | OUT2  
                                // OUT1 Drives PLD REMAP input 
                                // ROM is now relocated 0xF0000A

    // Configure UART0
    movb    0x80, @UART0_LCR    // Set UART0 LCR = Set DLAB
    movb    0x0C, @UART0_DLL    // Set UART0 DLL = 0x06 (38400 bps @ 7.32768MHz)
    movb    0x00, @UART0_IER    // Set UART0 IER = 0
    movb    0x03, @UART0_LCR    // Set UART0 LCR = Clear DLAB, Set n81 format 

    movb    '\r', @UART0_THR    // Output '\r' 
w1:
    tbitb   5, @UART0_LSR       // Check THRE 
    bfc     w1                  // Loop until set (Ready for next TX)

    movb    'L', @UART0_THR      // Output 'L' 
w2:
    tbitb   5, @UART0_LSR       // Check THRE 
    bfc     w2                  // Loop until set (Ready for next TX)


ramcheck1:
    movd    0x55AA9966, r0      // Load test pattern
    movd    r0, @STACK_TOP      // Write to top of stack
    cmpd    r0, @STACK_TOP      // check that it wrote correctly 
    bne     ramcheck1           // If not just loop 


    movb   '3', @UART0_THR      // Output '3' 
w3:
    tbitb   5, @UART0_LSR       // Check THRE 
    bfc     w3                  // Loop until set (Ready for next TX)

ramcheck2:
    movd    0xAA556699, r0      // Load test pattern
    movd    r0, @STACK_TOP      // Write to top of stack
    cmpd    r0, @STACK_TOP      // check that it wrote correctly 
    bne     ramcheck2           // If not just loop 


    movb   '2', @UART0_THR      // Output '2' 
w4:
    tbitb   5, @UART0_LSR       // Check THRE 
    bfc     w4                  // Loop if not set

    movb   'K', @UART0_THR      // Output 'K'


load_monitor:
    lprd sp,STACK_TOP           // Set supervisor stack pointer  
    jsr _cstart                 // Call C entry point 

    br start                    // We should never get here, but if we do restart
