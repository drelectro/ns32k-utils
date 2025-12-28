    .text
    .globl start
    .extern _cstart

start:
    lprd sp,0x3fffc     // optional - set stack to top of RAM  
    jsr _cstart         // Call C entry point 
    rxp 0               // return to TDS 

//1:
//    br 1b               // never return 
