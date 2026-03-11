# !/bin/sh
# Build script for freestanding test program for NS32k

# Build freestanding test program for NS32k
ns32k-pc532-netbsd-gcc -c start.S -o start.o
ns32k-pc532-netbsd-gcc -c wtf-ide.c -ffreestanding -o wtf-ide.o 

# Link to binary using freestanding linker script
ns32k-pc532-netbsd-ld -nostdlib -T freestanding.ldscript -o a.out start.o wtf-ide.o
ns32k-pc532-netbsd-objcopy -O binary a.out a.bin

cp a.bin /mnt/c/Temp/wtf.bin

# Optional: generate objdump for inspection
#ns32k-pc532-netbsd-objdump -D -x a.out > a.objdump