# !/bin/sh
# Simple build script for NS32k

# Build  test program for NS32k
ns32k-pc532-netbsd-gcc -c hosted-test.c -o hosted-test.o 

# Link to binary using freestanding linker script
ns32k-pc532-netbsd-ld -o a.out hosted-test.o
ns32k-pc532-netbsd-objcopy -O binary a.out a.bin

# Convert binary to TDS hex format
 ../bin2tds a.bin a.tds

# Copy TDS file to Windows temp folder for loading into TDS emulator
cp a.tds /mnt/c/Temp/a.tds

# Optional: generate objdump for inspection
#ns32k-pc532-netbsd-objdump -D -x freestanding-test.o > freestanding-test.objdump