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
;	push bp
;	mov bp, sp
;	mov edi, [bp+10] ; first argument
;	xor esi, esi
;	mov si, [bp+8] ; second argument
;	xor ecx, ecx
;	mov cx, [bp+4] ; third argument
;	push es
	cli

	; Load GDT with one flat data segment at index 1
;	lgdt ds:FWORD PTR[_pgdt]

	; Enable protected mode
        mov eax, cr0
	or eax, 1
	mov cr0, eax

	; Note, we probably need to far jump to actually switch to
	; protected mode, for which we need to set up a suitable
	; code segment

	; Load entry 1 to ES
;	mov ax, 8
;        mov es, ax

;	db 66h
;	rep movsb ; es:[edi] <- ds:[esi], ecx times

	; Disable protected mode
	mov eax, cr0
	xor eax, 1
	mov cr0, eax

	sti
;	pop es
;	pop bp
	ret
_protmode_loadhigh ENDP

	END