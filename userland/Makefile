# userland/Makefile — Compilar programas de usuario
#
# Los programas se compilan como ELF estáticos para x86
# sin ninguna librería estándar.
#
# Uso:
#   make          → compila todos los programas
#   make hello    → compila solo hello

CC     = gcc
LD     = ld
CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pie -fno-pic \
         -nostdlib -Wall -O2
LDFLAGS = -m elf_i386 -nostdlib -T user.ld

PROGRAMS = hello.elf

all: $(PROGRAMS)

hello.elf: hello.o
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf