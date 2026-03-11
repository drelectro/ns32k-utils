
#define UART0_DLL 0xFE0000
#define UART0_THR 0xFE0000
#define UART0_IER 0xFE0004
#define UART0_LCR 0xFE000C
#define UART0_MCR 0xFE0010
#define UART0_LSR 0xFE0014


#define IDE_REG_LBA_0	0x00FEC00C
#define IDE_DEV_HEAD    0x00FEC018
#define IDE_STATUS      0x00FEC01C


    .text
    .globl start
    .extern _cstart

start:
    lprd sp,0x3fffc     // optional - set stack to top of RAM  

    movb '*', r0
    jsr putc

    movb 0xA0, @IDE_DEV_HEAD
    

    movb 'D', r0
    jsr putc
    movb 'H', r0
    jsr putc
    movb '=', r0
    jsr putc
    movb ' ', r0
    jsr putc

    movb @IDE_DEV_HEAD, r0
    jsr outcharhex

    movb '\r', r0
    jsr putc
    movb '\n', r0
    jsr putc

    movb 'S', r0
    jsr putc
    movb 'T', r0
    jsr putc
    movb '=', r0
    jsr putc
    movb ' ', r0
    jsr putc

    movb @IDE_STATUS, r0
    jsr outcharhex

    movb '\r', r0
    jsr putc
    movb '\n', r0
    jsr putc


    movb 'L', r0
    jsr putc
    movb '0', r0
    jsr putc
    movb '=', r0
    jsr putc
    movb ' ', r0
    jsr putc

    movb 0xAA, @IDE_REG_LBA_0
    movb @IDE_REG_LBA_0, r0
    jsr outcharhex


    movb '\r', r0
    jsr putc
    movb '\n', r0
    jsr putc



    //jsr _cstart         // Call C entry point 
    //rxp 0               // return to TDS 

waitk:
    tbitb   0, @UART0_LSR  // Check Data Ready
    bfc     waitk
    movb    @UART0_THR, r0 // Read the byte, clear the bit
    br start               // never return 

outcharhex:
    enter	[r1],0
	movb	r0,r1
	movb	r1,r0
	lshb	-4,r0		# high nibble
	bsr	outnibble
	movb	r1,r0
	andb	0x0F,r0		# low nibble
	bsr	outnibble
	exit	[r1]
	ret	0

outnibble:
    andb	0x0F,r0
    addb	'0',r0
    cmpb	r0,'9'
    ble	outnib_digit
    addqb	7,r0
outnib_digit:
    jsr	putc
    ret	0

outhex16:
    # Print r0 as 8 hex digits
    enter	[r2],0
    movd	r0,r2
	br 		outhex16a
outhex32:
    # Print r0 as 8 hex digits
    enter	[r2],0
    movd	r0,r2
    movd	r2,r0
    lshd	-24,r0
    bsr	outcharhex
    movd	r2,r0
    lshd	-16,r0
    bsr	outcharhex
outhex16a:
    movd	r2,r0
    lshd	-8,r0
    bsr	outcharhex
    movd	r2,r0
    bsr	outcharhex
    exit	[r2]
    ret	0

putc:
    movb    r0, @UART0_THR      // Output byte 
w1:
    tbitb   5, @UART0_LSR       // Check THRE 
    bfc     w1                  // Loop until set (Ready for next TX)

    ret     0
