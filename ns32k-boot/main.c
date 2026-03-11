/*
 * This is a simple first stage bootloader and monitor for the NS32032 processor.
 * It is designed for use with my ns32k-104 hardware.
 * It provides a simple command line interface over the serial port for inspecting
 * and modifying memory, as well as reading the DIP switch for HW testing.
 *
 * 
 *
 * Mike Cornelius 14-02-2026
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

#define X_SOH 0x01
#define X_STX 0x02
#define X_EOT 0x04
#define X_ACK 0x06
#define X_NAK 0x15
#define X_CAN 0x18
#define X_CRCCHR 'C'

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
void copy_cstr (char *dst, const char *src, unsigned int dst_size);
int op_abort_requested ();
void cmd_md (unsigned int start, unsigned int length);
void cmd_mm (char width, unsigned int addr, unsigned int data, unsigned int length);
void cmd_mf (char width, unsigned int start, unsigned int data, unsigned int length);
void cmd_mt (unsigned int start, unsigned int length);
void cmd_go (unsigned int addr);
void cmd_rx (unsigned int addr);
int getc_timeout (unsigned int loops, unsigned char *out);
unsigned short xmodem_crc16 (const unsigned char *data, unsigned int len);
int xmodem_recv (unsigned char *dest, unsigned int *rx_len);
void print_usage ();

int cstart (void) 
{
   int run = 1;
   int i = 0;
   char buf[64];
   char last_cmd[64];
   int has_last_cmd = 0;

   last_cmd[0] = 0;

   puts ("\r\nNS32032 loader V0.1 - 14-02-2026\r\n");
   if ((((*(volatile unsigned char *)DIPSW_BASE) ^ 0xFF)) & 0x01) {
      puts("DIP switch bit 0 is ON:\r\n");
      cmd_go(0xF02000); // Jump to second stage bootloader at 0xF02000
      return 0; // Return to start.S which will restart
   }
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
            char *arg4;
            char exec_line[64];
            char raw_line[64];
            unsigned int repeat_depth = 0;
            unsigned int start;
            unsigned int length;
            unsigned int addr;
            unsigned int data;
            char width;

            if (i > 0 && (buf[i - 1] == '\r' || buf[i - 1] == '\n')) {
               i--;
            }
            buf[i] = 0;

            copy_cstr(exec_line, buf, sizeof(exec_line));

parse_command:
            copy_cstr(raw_line, exec_line, sizeof(raw_line));

            cursor = exec_line;
            cmd = next_token(&cursor);

            if (cmd == 0) {
               // Ignore empty lines
            } 
            else if (streq(cmd, "!")) {
               if (!has_last_cmd) {
                  puts("\r\nNo previous command");
               } else if (repeat_depth != 0u) {
                  puts("\r\nRepeat recursion blocked");
               } else {
                  repeat_depth = 1u;
                  copy_cstr(exec_line, last_cmd, sizeof(exec_line));
                  puts("\r\n");
                  puts(exec_line);
                  goto parse_command;
               }
            }
            else if (streq(cmd, "q")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               run = 0;
               return 0; // Return to start.S which will restart
            } 
            else if (streq(cmd, "s")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               puts ("\r\nDIP Switch = ");
               puthex ((*(volatile unsigned char *)DIPSW_BASE) ^ 0xFF); // Invert bits for display
            } 
            else if (streq(cmd, "md")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               arg1 = next_token(&cursor);
               arg2 = next_token(&cursor);
               if (arg1 == 0 || !parse_u32(arg1, &start)) {
                  puts ("\r\nUsage: md <start address> <length>");
               } 
               else {
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
            } 
            else if (streq(cmd, "mm")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               arg1 = next_token(&cursor);
               arg2 = next_token(&cursor);
               arg3 = next_token(&cursor);
               arg4 = next_token(&cursor);
               width = 'i';
               length = 1;

               if (arg1 == 0 || arg2 == 0) {
                  puts ("\r\nUsage: mm <b|w|i> <address> <data> [length]");
                  puts ("\r\n   or: mm <address> <data> [length]");
               } 
               else {
                  if (arg3 != 0 && arg4 != 0) {
                     if (arg1[1] != 0 || (arg1[0] != 'b' && arg1[0] != 'w' && arg1[0] != 'i')) {
                        puts ("\r\nInvalid width, use b/w/i");
                        i = 0;
                        puts ("\r\n> ");
                        continue;
                     }
                     width = arg1[0];
                     if (!parse_u32(arg2, &addr) || !parse_u32(arg3, &data) || !parse_u32(arg4, &length)) {
                        puts ("\r\nInvalid address or data");
                        i = 0;
                        puts ("\r\n> ");
                        continue;
                     }
                  } 
                  else if (arg3 != 0) {
                     if (arg1[1] == 0 && (arg1[0] == 'b' || arg1[0] == 'w' || arg1[0] == 'i')) {
                        width = arg1[0];
                        if (!parse_u32(arg2, &addr) || !parse_u32(arg3, &data)) {
                           puts ("\r\nInvalid address or data");
                           i = 0;
                           puts ("\r\n> ");
                           continue;
                        }
                     } else {
                        if (!parse_u32(arg1, &addr) || !parse_u32(arg2, &data) || !parse_u32(arg3, &length)) {
                           puts ("\r\nInvalid address, data or length");
                           i = 0;
                           puts ("\r\n> ");
                           continue;
                        }
                     }
                  }
                  else {
                     if (!parse_u32(arg1, &addr) || !parse_u32(arg2, &data)) {
                        puts ("\r\nInvalid address or data");
                        i = 0;
                        puts ("\r\n> ");
                        continue;
                     }
                  }
                  cmd_mm(width, addr, data, length);
               }
            } 
            else if (streq(cmd, "mf")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               arg1 = next_token(&cursor);
               arg2 = next_token(&cursor);
               arg3 = next_token(&cursor);
               arg4 = next_token(&cursor);
               width = 'i';

               if (arg1 == 0 || arg2 == 0 || arg3 == 0) {
                  puts ("\r\nUsage: mf <b|w|i> <address> <data> <length>");
                  puts ("\r\n   or: mf <address> <data> <length>");
               }
               else if (arg4 != 0) {
                  if (arg1[1] != 0 || (arg1[0] != 'b' && arg1[0] != 'w' && arg1[0] != 'i')) {
                     puts ("\r\nInvalid width, use b/w/i");
                  }
                  else {
                     width = arg1[0];
                     if (!parse_u32(arg2, &start) || !parse_u32(arg3, &data) || !parse_u32(arg4, &length)) {
                        puts ("\r\nInvalid address, data or length");
                     }
                     else {
                        cmd_mf(width, start, data, length);
                     }
                  }
               }
               else {
                  if (!parse_u32(arg1, &start) || !parse_u32(arg2, &data) || !parse_u32(arg3, &length)) {
                     puts ("\r\nInvalid address, data or length");
                  }
                  else {
                     cmd_mf(width, start, data, length);
                  }
               }
            }
            else if (streq(cmd, "mt")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               arg1 = next_token(&cursor);
               arg2 = next_token(&cursor);
               if (arg1 == 0 || arg2 == 0 || !parse_u32(arg1, &start) || !parse_u32(arg2, &length)) {
                  puts ("\r\nUsage: mt <start address> <length>");
               } 
               else {
                  cmd_mt(start, length);
               }
            } 
            else if (streq(cmd, "rx")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               arg1 = next_token(&cursor);
               if (arg1 == 0 || !parse_u32(arg1, &addr)) {
                  puts ("\r\nUsage: rx <address>");
               } 
               else {
                  cmd_rx(addr);
               }
            } 
            else if (streq(cmd, "go")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               arg1 = next_token(&cursor);
               if (arg1 == 0 || !parse_u32(arg1, &addr)) {
                  puts ("\r\nUsage: go <address>");
               } 
               else {
                  cmd_go(addr);
               }
            } 
            else if (streq(cmd, "h") || streq(cmd, "help")) {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
               print_usage();
            } 
            else {
               copy_cstr(last_cmd, raw_line, sizeof(last_cmd));
               has_last_cmd = 1;
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

void copy_cstr (char *dst, const char *src, unsigned int dst_size) {
   unsigned int i = 0;

   if (dst_size == 0u) {
      return;
   }
   while (i < (dst_size - 1u) && src[i] != 0) {
      dst[i] = src[i];
      i++;
   }
   dst[i] = 0;
}

int op_abort_requested () {
   if (isRxReady()) {
      char c = getc();
      if (c == 0x1B || c == 0x03) {
         puts("\r\nAborted");
         return 1;
      }
   }
   return 0;
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
   unsigned int end;
   unsigned int lines = 0;

   if (length == 0u) {
      puts("\r\nLength must be >= 1");
      return;
   }

   end = start + length;
   if (end < start) {
      puts("\r\nRange overflow");
      return;
   }

   while (addr < end) {
      unsigned int line;
      unsigned int line_addr = addr;
      unsigned char ascii[16];
      unsigned int count = 0;

      if ((lines & 0x0Fu) == 0u && op_abort_requested()) {
         return;
      }
      lines++;

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

void cmd_mm (char width, unsigned int addr, unsigned int data, unsigned int length) {
   unsigned int index;
   unsigned int step;
   unsigned int bytes;

   if (length == 0u) {
      puts("\r\nLength must be >= 1");
      return;
   }

   if (width == 'b') {
      step = 1u;
      data &= 0xFFu;
   } else if (width == 'w') {
      step = 2u;
      data &= 0xFFFFu;
      if ((addr & 1u) != 0u) {
         puts("\r\nUnaligned address for width 'w'");
         return;
      }
   } else if (width == 'i') {
      step = 4u;
      if ((addr & 3u) != 0u) {
         puts("\r\nUnaligned address for width 'i'");
         return;
      }
   } else {
      puts("\r\nInvalid width, use b/w/i");
      return;
   }

   if (length > (0xFFFFFFFFu / step)) {
      puts("\r\nRange overflow");
      return;
   }
   bytes = length * step;
   if ((addr + bytes) < addr) {
      puts("\r\nRange overflow");
      return;
   }

   puts("\r\n[");
   putc(width);
   puts("] fill ");
   puthex32(addr);
   puts(" <= ");
   puthex32(data);
   puts(" len=");
   puthex32(length);

   if (width == 'b') {
      for (index = 0; index < length; index++) {
         if ((index & 0xFFu) == 0u && op_abort_requested()) {
            return;
         }
         *(volatile unsigned char *)(addr + index) = (unsigned char)data;
      }
      puts("  readback=");
      puthex(*(volatile unsigned char *)addr);
   } else if (width == 'w') {
      for (index = 0; index < length; index++) {
         if ((index & 0xFFu) == 0u && op_abort_requested()) {
            return;
         }
         *(volatile unsigned short *)(addr + (index * 2u)) = (unsigned short)data;
      }
      puts("  readback=");
      puthex((unsigned char)((*(volatile unsigned short *)addr >> 8) & 0xFF));
      puthex((unsigned char)(*(volatile unsigned short *)addr & 0xFF));
   } else {
      for (index = 0; index < length; index++) {
         if ((index & 0xFFu) == 0u && op_abort_requested()) {
            return;
         }
         *(volatile unsigned int *)(addr + (index * 4u)) = data;
      }
      puts("  readback=");
      puthex32(*(volatile unsigned int *)addr);
   }
}

void cmd_mf (char width, unsigned int start, unsigned int data, unsigned int length) {
   unsigned int end;
   unsigned int addr;
   unsigned int matches = 0;
   unsigned int scan_count = 0;
   unsigned int step;

   puts("\r\n[");
   putc(width);
   puts("] find ");
   puthex32(data);
   puts(" from ");
   puthex32(start);
   puts(" len=");
   puthex32(length);

   if (width == 'b') {
      step = 1u;
      data &= 0xFFu;
   } else if (width == 'w') {
      step = 2u;
      data &= 0xFFFFu;
      if ((start & 1u) != 0u) {
         puts("\r\nUnaligned address for width 'w'");
         return;
      }
   } else {
      if (width != 'i') {
         puts("\r\nInvalid width, use b/w/i");
         return;
      }
      step = 4u;
      if ((start & 3u) != 0u) {
         puts("\r\nUnaligned address for width 'i'");
         return;
      }
   }

   if (length < step) {
      puts("\r\nLength too small for width");
      return;
   }

   end = start + length;
   if (end < start) {
      puts("\r\nRange overflow");
      return;
   }
   for (addr = start; addr <= (end - step); addr += step) {
      unsigned int value;

      if ((scan_count & 0xFFu) == 0u && op_abort_requested()) {
         return;
      }
      scan_count++;

      if (width == 'b') {
         value = (unsigned int)(*(volatile unsigned char *)addr);
      } else if (width == 'w') {
         value = (unsigned int)(*(volatile unsigned short *)addr);
      } else {
         value = *(volatile unsigned int *)addr;
      }

      if (value == data) {
         puts("\r\n");
         puthex32(addr);
         matches++;
      }
   }

   puts("\r\nMatches=");
   puthex32(matches);
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


void cmd_go (unsigned int addr) {
   void (*entry)(void) = (void (*)(void))addr;

   if ((addr & 1u) != 0u) {
      puts("\r\nInvalid address (must be even)");
      return;
   }
   if (addr == 0u) {
      puts("\r\nInvalid address (zero)");
      return;
   }

   puts("\r\nJumping to ");
   puthex32(addr);
   puts("\r\n");

   entry();

   puts("\r\nProgram returned\r\n");
}

int getc_timeout (unsigned int loops, unsigned char *out) {
   while (loops--) {
      if (isRxReady()) {
         *out = (unsigned char)getc();
         return 1;
      }
   }
   return 0;
}

unsigned short xmodem_crc16 (const unsigned char *data, unsigned int len) {
   unsigned short crc = 0;
   unsigned int i;

   while (len--) {
      crc ^= (unsigned short)(*data++) << 8;
      for (i = 0; i < 8; i++) {
         if (crc & 0x8000) {
            crc = (unsigned short)((crc << 1) ^ 0x1021);
         } else {
            crc <<= 1;
         }
      }
   }
   return crc;
}

int xmodem_recv (unsigned char *dest, unsigned int *rx_len) {
   unsigned char pkt[1024];
   unsigned char ch;
   unsigned char blk;
   unsigned char blk_inv;
   unsigned int block_len;
   unsigned int got = 0;
   unsigned char expect = 1;
   unsigned int tries = 0;
   int started = 0;

   putc(X_CRCCHR); // request CRC mode immediately

   while (1) {
      if (!getc_timeout(60000u, &ch)) {
         if (!started) {
            if (tries++ > 120) {
               putc(X_CAN);
               putc(X_CAN);
               return -1; // timeout waiting for sender
            }
            putc(X_CRCCHR);
         } else {
            if (tries++ > 40) {
               putc(X_CAN);
               putc(X_CAN);
               return -4; // timeout mid-transfer
            }
            putc(X_NAK);
         }
         continue;
      }
      tries = 0;

      if (ch == X_SOH || ch == X_STX) {
         unsigned int i;
         unsigned short crc_calc;
         unsigned short crc_rx;

         started = 1;
         block_len = (ch == X_SOH) ? 128u : 1024u;

         if (!getc_timeout(100000u, &blk) || !getc_timeout(100000u, &blk_inv)) {
            putc(X_NAK);
            continue;
         }
         if ((unsigned char)(blk + blk_inv) != 0xFF) {
            putc(X_NAK);
            continue;
         }

         for (i = 0; i < block_len; i++) {
            if (!getc_timeout(100000u, &pkt[i])) {
               putc(X_NAK);
               goto next_packet;
            }
         }

         if (!getc_timeout(100000u, &ch)) {
            putc(X_NAK);
            continue;
         }
         crc_rx = (unsigned short)ch << 8;
         if (!getc_timeout(100000u, &ch)) {
            putc(X_NAK);
            continue;
         }
         crc_rx |= (unsigned short)ch;

         crc_calc = xmodem_crc16(pkt, block_len);
         if (crc_calc != crc_rx) {
            putc(X_NAK);
            continue;
         }

         if (blk == expect) {
            for (i = 0; i < block_len; i++) {
               dest[got++] = pkt[i];
            }
            expect++;
            putc(X_ACK);
         } else if (blk == (unsigned char)(expect - 1)) {
            putc(X_ACK); // duplicate block retransmit
         } else {
            putc(X_CAN);
            putc(X_CAN);
            return -2; // unexpected block number
         }
      } else if (ch == X_EOT) {
         putc(X_ACK);
         *rx_len = got;
         return 0;
      } else if (ch == X_CAN) {
         putc(X_ACK);
         return -3; // cancelled by sender
      }

next_packet:
      ;
   }
}

void cmd_rx (unsigned int addr) {
   unsigned int received = 0;
   int rc;

   while (isRxReady()) {
      (void)getc();
   }

   puts("\r\nXMODEM-CRC receive to ");
   puthex32(addr);
   puts("\r\nSend file now...\r\n");

   rc = xmodem_recv((unsigned char *)addr, &received);

   puts("\r\nrx status=");
   puthex32((unsigned int)rc);
   puts(" bytes=");
   puthex32(received);
}

void print_usage () {
   puts("Commands:\r\n");
   puts("  md <start> <length>   - dump memory (default length=0x100)\r\n");
   puts("  mm [b|w|i] <addr> <data> [length] - modify memory\r\n");
   puts("  mf [b|w|i] <addr> <data> <length> - find in memory\r\n");
   puts("  mt <start> <length>   - memory test (destructive)\r\n");
   puts("  rx <addr>             - XMODEM-CRC receive binary\r\n");
   puts("  go <addr>             - jump to loaded program\r\n");
   puts("  s                     - read DIP switch\r\n");
   puts("  q                     - restart\r\n");
   puts("  !                     - repeat previous command\r\n");
   puts("  Note: ESC or Ctrl-C aborts long md/mm/mf operations\r\n");
}
