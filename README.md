# DOSMBoot

DOS application that loads a multiboot2-compliant (HelenOS) OS image (using pieces of code that run in 386 protected mode) and starts it.

## Compiling DOSMBoot

To compile you need Borland C++ 3.1 IDE (i.e. Borland C compiler and Turbo Assembler).
Either use the project file included in the repo,
or create a new project and add all the .c and .asm files,
compile with small memory model and all the default compilation options.

## Preparing HelenOS image
You need to build HelenOS for ia32. Suggest selecting 486 processor and disabling SMP.
You *must* disable framebuffer, because DOSMBoot does not provide one.

## Testing DOSMBoot
You need to add a kernel.elf file to the directory where DOSMBOOT.EXE is located.
You need to create a subdirectory named MODULES and add all the HelenOS boot modules
to it.

### DosBox
Default memory size for DosBox 0.76.0 seems to be 16 MB. This is unfortunately not
enough to load the entire OS from RAM disk. You need to edit $HOME/.config/dosbox/dosbox-xxx.conf
and increase memsize to at least 128.

Mount the directory containing DOSMBOOT, chdir to it and run DOSMBOOT.EXE.

### Qemu
It's a good idea to have mtools package and a Qemu virtual machine with DOS. Prepare a floppy image with dosmboot

`$ dd if=/dev/zero of=floppy.img bs=1k count=1440`  
`$ mkfs -t msdos floppy.img`  
`$ mcopy -i floppy.img /path/kernel.elf ::`  
`$ mmd -i floppy.img ::modules`  
`$ mcopy -i floppy.img /path/<module-name> ::modules`  
`$ qemu ... -drive if=floppy,index=0,format=raw,file=floppy.img`  


`C:\> A:`  
`A:\> DOSMBOOT`  
