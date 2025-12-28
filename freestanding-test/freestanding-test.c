/*
 * This is a simple "freestanding" test program for the NS32032 processor.
 * It is designed to be loaded and run by the TDS debugger.
 * In this case freestanding means frestanding from a compiler perspective, no CRT0 ETC  
 * It does however depend on a minimal runtime environment provided by TDS.
 *
 * The program provides a simple UART0 based console interface.
 * It waits for user input from the UART0 serial port, and echoes back characters.
 * It recognizes a few simple commands:
 *   '1' - prints "Test 1"
 *   '2' - prints "Test 2"
 *   'q' - exits the program and returns control to TDS
 * Any other input results in "Unknown command" message.
 * The program uses memory-mapped I/O to interact with the UART0 hardware.
 * It directly accesses UART0 registers to send and receive data.
 *
 * This program is intended to validate a basic freestanding toolchain for
 * the NS32032 architecture.
 *
 * Mike Cornelius 28-12-2025
 *
 */


#define UART0_BASE 0xFE0000
#define UART0_RBR  (UART0_BASE + 0x00) // Receiver Buffer Register
#define UART0_THR  (UART0_BASE + 0x00) // Transmitter Holding Register
#define UART0_IER  (UART0_BASE + 0x04) // Interrupt Enable Register
#define UART0_IIR  (UART0_BASE + 0x08) // Interrupt Identification Register
#define UART0_FCR  (UART0_BASE + 0x08) // FIFO Control Register
#define UART0_LCR  (UART0_BASE + 0x0C) // Line Control Register
#define UART0_MCR  (UART0_BASE + 0x10) // Modem Control Register
#define UART0_LSR  (UART0_BASE + 0x14) // Line Status Register
#define UART0_MSR  (UART0_BASE + 0x18) // Modem Status Register
#define UART0_SCR  (UART0_BASE + 0x1C) // Scratch Register
#define UART0_DLL  (UART0_BASE + 0x00) // Divisor Latch Low
#define UART0_DLH  (UART0_BASE + 0x04) // Divisor Latch High


int putc (char c);
int puts (char *s);
int isRxReady ();
char getc ();

int cstart (void) {

    int run = 1;

   puts ("\r\nHellorld - NS32032 - freestanding - V1.0 - 28-12-2025\r\n");

   puts ("> ");
   while (run) {
      
      if (isRxReady()) {
         char c = getc();
         putc (c); // Echo back

         switch (c) {
            case 'q':
               run = 0;  // Exit on 'q'
               return 0; // Return to start.S whuch will return to TDS
               break;

            case '1':
               puts ("\r\nTest 1\r\n");
               break;

            case '2':
               puts ("\r\nTest 2\r\n");
               break;

            default:
               puts ("\r\nUnknown command\r\n");
               break;
         }
         puts (">  ");
      }
   }

}
 

int putc (char c) {
   while ((*(volatile unsigned char *)UART0_LSR & 0x20) == 0);
   *(volatile unsigned char *)UART0_THR = c;
   return c;
}

int puts (char *s) {
   while (*s) {
      putc (*s++);
   }
   return 0;
}

int isRxReady () {
   return (*(volatile unsigned char *)UART0_LSR & 0x01);
}
char getc () {
   while (!isRxReady());
   return *(volatile unsigned char *)UART0_RBR;
}
