all: rom_videx.o

rom_%.o: %.rom
	arm-none-eabi-objcopy -I binary -O elf32-littlearm -B arm \
	--rename-section .data=.rodata,readonly,data,contents,alloc \
	--redefine-sym _binary_$*_rom_start=rom_$* \
	--redefine-sym _binary_$*_rom_end=rom_$*_end \
	--redefine-sym _binary_$*_rom_size=rom_$*_size \
	$< $@

