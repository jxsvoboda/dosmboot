;
; Protected mode routines for Dosmboot
;

	P386
	MODEL USE16 SMALL
	.DATA
	EXTRN _pgdt:PTR
	.CODE

	PUBLIC _protmode_is_prot
	PUBLIC _protmode_loadhigh
	PUBLIC _protmode_os_exec

;
; Test if we are running in protected mode
;
; @return 1 if running in protected mode, 0 otherwise
;
_protmode_is_prot PROC
	smsw ax
	and ax, 01h
	ret
_protmode_is_prot ENDP

;
; Copy memory block to high memory
;
; @param uint32_t src Destination address (linear)
; @param const void *src Source address (offset in the default data segment)
; @param size_t n Number of bytes to copy
;
_protmode_loadhigh PROC
	push bp
	mov bp, sp
	mov edi, [bp+4] ; first argument (32 bits)
	xor esi, esi
	mov si, [bp+8] ; second argument (16 bits)
	xor ecx, ecx
	mov cx, [bp+10] ; third argument (16 bits)
	cli
	push ds
	push es

	; Load GDT with one flat data segment at index 1
	lgdt ds:FWORD PTR[_pgdt]

	; Enable protected mode
        mov eax, cr0
	or eax, 1
	mov cr0, eax

	; Jump to 16-bit protected code segment

	db 0eah ; far jump
	dw offset now_in_prot
	dw 08h ; GDT entry 1, RPL = 0

now_in_prot:

	; Load entry 2 (16-bit data segment) to DS
	mov ax, 16
	mov ds, ax

	; Load entry 3 (32-bit flat data segment) to ES
	mov ax, 24
	mov es, ax

	; Cannot write as rep movsb because we need 32-bit addresses
	rep movs byte ptr es:[edi], byte ptr ds:[esi]

	; Disable protected mode
	mov eax, cr0
	xor eax, 1
	mov cr0, eax

	; Jump to real-mode address
	db 0eah ; far jump
	dw offset now_in_real
	dw @code
now_in_real:

	pop es
	pop ds
	sti
	pop bp
	ret
_protmode_loadhigh ENDP

;
; Execute OS image
;
; @param uint32_t mbptr Multiboot information structure address (linear)
; @param uint32_t start OS start address (linear)
;
_protmode_os_exec PROC
	push bp
	mov bp, sp

	; Multiboot information structure pointer
	mov ebx, [bp+4] ; first argument (32 bits)
	; Image entry point
	mov eax, [bp+8] ; second argument (32 bits)
	mov DWORD PTR cs:[entry_addr], eax

	; Interrupts must be disabled
	cli

	; Enable protected mode
        mov eax, cr0
	or eax, 1
	mov cr0, eax

	; Load entry 3 (32-bit flat data segment)
	mov ax, 24
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; Multiboot magic value for i386
	mov eax, 36d76289h

	; Jump straight to OS entry point, using flat code segment
	db 066h ; 32-bit
	db 0eah ; far jump
entry_addr:
	dd 0 ; fill in at run time
	dw 32 ; GDT entry 4, RPL = 0

	ret
_protmode_os_exec ENDP

	END
