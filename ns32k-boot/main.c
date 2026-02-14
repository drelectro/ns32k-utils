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

#define DIPSW_BASE 0xFE8000

int putc (char c);
int puts (char *s);
int isRxReady ();
char getc ();
void puthex (unsigned char val);
void puthex32 (unsigned int val);
void putcrlf ();
int streq (const char *a, const char *b);
int is_space (char c);
char *next_token (char **cursor);
int parse_u32 (const char *s, unsigned int *out);
void cmd_md (unsigned int start, unsigned int length);
void cmd_mm (char width, unsigned int addr, unsigned int data);
void cmd_mt (unsigned int start, unsigned int length);
void print_usage ();

int cstart (void) 
{
   int run = 1;
   int i = 0;
   char buf[64];

   puts ("\r\nNS32032 loader V0.1 - 14-02-2026\r\n");
   print_usage();

   puts ("> ");
   while (run) {
      
      if (isRxReady()) {
         char c = getc();

         if (c == '\b' || c == 0x7F) {
            if (i > 0) {
               i--;
               buf[i] = 0;
               putc('\b');
               putc(' ');
               putc('\b');
            }
            continue;
         }

         putc (c); // Echo back

         // Accumulate input until newline 
         // Ignore extra characters beyond buffer size
         if (i < sizeof(buf)-1) {
            buf[i++] = c;
            buf[i] = 0;
         } else {
             i = sizeof(buf)-1;   // Prevent overflow
         }
         if (c != '\r' && c != '\n') {
             continue;  // Continue accumulating input
         }  

         if (c == '\r' || c == '\n') {
            char *cursor;
            char *cmd;
            char *arg1;
            char *arg2;
            char *arg3;
            unsigned int start;
            unsigned int length;
            unsigned int addr;
            unsigned int data;
            char width;

            if (i > 0 && (buf[i - 1] == '\r' || buf[i - 1] == '\n')) {
               i--;
            }
            buf[i] = 0;

            cursor = buf;
            cmd = next_token(&cursor);

            if (cmd == 0) {
               // Ignore empty lines
            } else if (streq(cmd, "q")) {
               run = 0;
               return 0; // Return to start.S which will restart
            } else if (streq(cmd, "1")) {
               puts ("\r\nTest 1");
            } else if (streq(cmd, "2")) {
               puts ("\r\nTest 2");
            } else if (streq(cmd, "s")) {
               puts ("\r\nDIP Switch = ");
               puthex ((*(volatile unsigned char *)DIPSW_BASE) ^ 0xFF); // Invert bits for display
            } else if (streq(cmd, "md")) {
               arg1 = next_token(&cursor);
               arg2 = next_token(&cursor);
               if (arg1 == 0 || !parse_u32(arg1, &start)) {
                  puts ("\r\nUsage: md <start address> <length>");
               } else {
                  length = 0x100;
                  if (arg2 != 0) {
                     if (!parse_u32(arg2, &length)) {
                        puts ("\r\nInvalid length");
                        i = 0;
                        puts ("\r\n> ");
                        continue;
                     }
                  }
                  puts ("\r\n");
                  cmd_md(start, length);
               }
            } else if (streq(cmd, "mm")) {
               arg1 = next_token(&cursor);
               arg2 = next_token(&cursor);
               arg3 = next_token(&cursor);
               width = 'i';

               if (arg1 == 0 || arg2 == 0) {
                  puts ("\r\nUsage: mm <b|w|i> <address> <data>");
                  puts ("\r\n   or: mm <address> <data>");
               } else {
                  if (arg3 != 0) {
                     if (arg1[1] != 0 || (arg1[0] != 'b' && arg1[0] != 'w' && arg1[0] != 'i')) {
                        puts ("\r\nInvalid width, use b/w/i");
                        i = 0;
                        puts ("\r\n> ");
                        continue;
                     }
                     width = arg1[0];
                     if (!parse_u32(arg2, &addr) || !parse_u32(arg3, &data)) {
                        puts ("\r\nInvalid address or data");
                        i = 0;
                        puts ("\r\n> ");
                        continue;
                     }
                  } else {
                     if (!parse_u32(arg1, &addr) || !parse_u32(arg2, &data)) {
                        puts ("\r\nInvalid address or data");
                        i = 0;
                        puts ("\r\n> ");
                        continue;
                     }
                  }
                  cmd_mm(width, addr, data);
               }
            } else if (streq(cmd, "mt")) {
               arg1 = next_token(&cursor);
               arg2 = next_token(&cursor);
               if (arg1 == 0 || arg2 == 0 || !parse_u32(arg1, &start) || !parse_u32(arg2, &length)) {
                  puts ("\r\nUsage: mt <start address> <length>");
               } else {
                  cmd_mt(start, length);
               }
            } else if (streq(cmd, "h") || streq(cmd, "help")) {
               print_usage();
            } else {
               puts ("\r\nUnknown command");
            }

            i = 0;  // Reset input buffer index
            puts ("\r\n> ");
         }
      }
   }
   puts ("\r\nExiting...\r\n");
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

void puthex (unsigned char val) {
   const char *hex = "0123456789ABCDEF";
   putc (hex[(val >> 4) & 0x0F]);
   putc (hex[val & 0x0F]);
}

void puthex32 (unsigned int val) {
   puthex((unsigned char)((val >> 24) & 0xFF));
   puthex((unsigned char)((val >> 16) & 0xFF));
   puthex((unsigned char)((val >> 8) & 0xFF));
   puthex((unsigned char)(val & 0xFF));
}

void putcrlf () {
   putc('\r');
   putc('\n');
}

int streq (const char *a, const char *b) {
   while (*a && *b) {
      if (*a != *b) {
         return 0;
      }
      a++;
      b++;
   }
   return (*a == 0 && *b == 0);
}

int is_space (char c) {
   return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

char *next_token (char **cursor) {
   char *p;
   char *start;

   p = *cursor;
   while (*p && is_space(*p)) {
      p++;
   }
   if (*p == 0) {
      *cursor = p;
      return 0;
   }
   start = p;
   while (*p && !is_space(*p)) {
      p++;
   }
   if (*p) {
      *p = 0;
      p++;
   }
   *cursor = p;
   return start;
}

int parse_u32 (const char *s, unsigned int *out) {
   unsigned int base = 10;
   unsigned int v = 0;
   int any = 0;
   const char *p = s;

   if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
      base = 16;
      p += 2;
   } else {
      const char *q = p;
      while (*q) {
         if ((*q >= 'a' && *q <= 'f') || (*q >= 'A' && *q <= 'F')) {
            base = 16;
            break;
         }
         q++;
      }
   }

   while (*p) {
      unsigned int d;
      char c = *p;
      if (c >= '0' && c <= '9') {
         d = (unsigned int)(c - '0');
      } else if (base == 16 && c >= 'a' && c <= 'f') {
         d = (unsigned int)(c - 'a' + 10);
      } else if (base == 16 && c >= 'A' && c <= 'F') {
         d = (unsigned int)(c - 'A' + 10);
      } else {
         return 0;
      }
      if (d >= base) {
         return 0;
      }
      v = (v * base) + d;
      any = 1;
      p++;
   }

   if (!any) {
      return 0;
   }
   *out = v;
   return 1;
}

void cmd_md (unsigned int start, unsigned int length) {
   unsigned int addr = start;
   unsigned int end = start + length;

   while (addr < end) {
      unsigned int line;
      unsigned int line_addr = addr;
      unsigned char ascii[16];
      unsigned int count = 0;

      puthex32(line_addr);
      puts(": ");

      for (line = 0; line < 16; line++) {
         if (addr < end) {
            unsigned char v = *(volatile unsigned char *)addr;
            puthex(v);
            ascii[count++] = v;
            addr++;
         } else {
            puts("  ");
         }

         putc(' ');
         if (line == 7) {
            putc(' ');
         }
      }

      putc('|');
      for (line = 0; line < count; line++) {
         unsigned char ch = ascii[line];
         if (ch >= 32 && ch <= 126) {
            putc((char)ch);
         } else {
            putc('.');
         }
      }
      for (; line < 16; line++) {
         putc(' ');
      }
      putc('|');
      putcrlf();
   }
}

void cmd_mm (char width, unsigned int addr, unsigned int data) {
   puts("\r\n[");
   putc(width);
   puts("] ");
   puthex32(addr);
   puts(" <= ");
   puthex32(data);

   if (width == 'b') {
      *(volatile unsigned char *)addr = (unsigned char)data;
      puts("  readback=");
      puthex(*(volatile unsigned char *)addr);
   } else if (width == 'w') {
      *(volatile unsigned short *)addr = (unsigned short)data;
      puts("  readback=");
      puthex((unsigned char)((*(volatile unsigned short *)addr >> 8) & 0xFF));
      puthex((unsigned char)(*(volatile unsigned short *)addr & 0xFF));
   } else {
      *(volatile unsigned int *)addr = data;
      puts("  readback=");
      puthex32(*(volatile unsigned int *)addr);
   }
}

void cmd_mt (unsigned int start, unsigned int length) {
   unsigned int words = length / 4;
   unsigned int index;
   volatile unsigned int *p = (volatile unsigned int *)start;
   unsigned int errors = 0;

   puts("\r\nMemory test (destructive) start=");
   puthex32(start);
   puts(" len=");
   puthex32(length);

   if (words == 0) {
      puts("\r\nLength too small (need >= 4 bytes)");
      return;
   }

   for (index = 0; index < words; index++) {
      p[index] = 0x55555555u;
      if (p[index] != 0x55555555u) {
         errors++;
      }
      p[index] = 0xAAAAAAAAu;
      if (p[index] != 0xAAAAAAAAu) {
         errors++;
      }
      p[index] = (start + (index * 4));
      if (p[index] != (start + (index * 4))) {
         errors++;
      }
   }

   puts("\r\nTest complete, errors=");
   puthex32(errors);
}

void print_usage () {
   puts("Commands:\r\n");
   puts("  md <start> <length>   - dump memory (default length=0x100)\r\n");
   puts("  mm <b|w|i> <addr> <data> or mm <addr> <data>\r\n");
   puts("  mt <start> <length>   - memory test (destructive)\r\n");
   puts("  s                     - read DIP switch\r\n");
   puts("  q                     - restart\r\n");
}
