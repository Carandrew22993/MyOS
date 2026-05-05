# Makefile — Sistema de compilación de myOS
# Linux Mint / Ubuntu / Debian — no requiere cross-compiler
#
# Comandos:
#   make          → compila el kernel (myos.bin)
#   make iso      → genera myos.iso booteable con GRUB
#   make run      → corre el kernel en QEMU directamente
#   make run-iso  → corre el ISO en QEMU
#   make clean    → elimina archivos generados

# ── Herramientas ────────────────────────────────────────────────────────────
CC   = gcc
AS   = nasm
LD   = ld
GRUB = grub-mkrescue

# ── Flags ───────────────────────────────────────────────────────────────────
CFLAGS = \
	-std=gnu99 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pie \
	-fno-pic \
	-m32 \
	-Wall \
	-Wextra \
	-O2

ASFLAGS = -f elf32

LDFLAGS = \
	-T linker.ld \
	-nostdlib \
	-m elf_i386

# ── Archivos fuente ─────────────────────────────────────────────────────────
ASM_SOURCES = boot/boot.asm boot/gdt_asm.asm boot/idt_asm.asm
C_SOURCES   = kernel/kernel.c kernel/gdt.c kernel/idt.c kernel/timer.c kernel/keyboard.c kernel/pmm.c kernel/paging.c kernel/kheap.c kernel/scheduler.c

ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)
C_OBJECTS   = $(C_SOURCES:.c=.o)
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS)

KERNEL = myos.bin
ISO    = myos.iso

# ── Reglas ──────────────────────────────────────────────────────────────────
.PHONY: all iso run run-iso clean

all: $(KERNEL)

$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo ""
	@echo "  Kernel compilado: $(KERNEL)"
	@echo "  Tamanio: $$(du -h $(KERNEL) | cut -f1)"
	@echo ""

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# ── ISO booteable ───────────────────────────────────────────────────────────
iso: $(KERNEL)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/$(KERNEL)
	cp grub.cfg isodir/boot/grub/grub.cfg
	$(GRUB) -o $(ISO) isodir
	@echo "ISO creado: $(ISO)"

# ── QEMU ────────────────────────────────────────────────────────────────────
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL)

run-iso: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

# ── Limpieza ────────────────────────────────────────────────────────────────
clean:
	rm -f $(OBJECTS) $(KERNEL) $(ISO)
	rm -rf isodir