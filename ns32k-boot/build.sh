# !/bin/sh
# NS32k first stage bootloader

# NS32k first stage bootloader 
ns32k-pc532-netbsd-gcc -c start.S -o start.o
ns32k-pc532-netbsd-gcc -c main.c -ffreestanding -o main.o

# Link to binary using baremetal linker script
ns32k-pc532-netbsd-ld -nostdlib -T baremetal.ldscript -o a.out start.o main.o
ns32k-pc532-netbsd-objcopy -O binary a.out a.bin

ns32k-pc532-netbsd-objcopy -I binary -O binary --interleave=4 --byte=0 a.bin eprom0.bin
ns32k-pc532-netbsd-objcopy -I binary -O binary --interleave=4 --byte=1 a.bin eprom1.bin
ns32k-pc532-netbsd-objcopy -I binary -O binary --interleave=4 --byte=2 a.bin eprom2.bin
ns32k-pc532-netbsd-objcopy -I binary -O binary --interleave=4 --byte=3 a.bin eprom3.bin

# Copy binary  file to Windows temp folder for loading into emulator
cp *.bin /mnt/c/Temp


# Optional: generate objdump for inspection
#ns32k-pc532-netbsd-objdump -D -x a.out > a.objdump

