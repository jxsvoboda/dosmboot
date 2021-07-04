#include <dos.h>
#include <stdint.h>
#include <stdio.h>

#include "a20.h"

int a20_check(void)
{
	uint8_t old;
	uint8_t far *hi;
	uint8_t far *lo;
	int a20_enabled;

	/* This is a memory location only used by ROM BASIC */
	lo = MK_FP(0x0, 0x510);
	/* 0xffff0 + 0x520 = 0x100510 is an alias iff A20 is off */
	hi = MK_FP(0xffff, 0x520);

	old = *lo;
        *lo = 0x00;
	*hi = 0xff;

	printf("*lo=0x%x *hi=0x%x\n", *lo, *hi);
	a20_enabled = (*lo != *hi);
	*lo = old;

	return a20_enabled;
}
