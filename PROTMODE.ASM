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
	mov edi, [bp+10] ; first argument
	xor esi, esi
	mov si, [bp+8] ; second argument
	xor ecx, ecx
	mov cx, [bp+4] ; third argument
	cli
	push ds
	push es

	; Load GDT with one flat data segment at index 1
	lgdt ds:FWORD PTR[_pgdt]

	; Enable protected mode
        mov eax, cr0
	or eax, 1
	mov cr0, eax

	; Note, we probably need to far jump to actually switch to
	; protected mode, for which we need to set up a suitable
	; code segment

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

	db 66h
	rep movsb ; es:[edi] <- ds:[esi], ecx times

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

	END