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

	a20_enabled = (*lo != *hi);
	*lo = old;

	return a20_enabled;
}

void a20_disable_bios(void)
{
	union REGS iregs, oregs;

	iregs.x.ax = 0x2400;
	int86(0x15, &iregs, &oregs);
}

void a20_enable_bios(void)
{
	union REGS iregs, oregs;

	iregs.x.ax = 0x2401;
	int86(0x15, &iregs, &oregs);
}

static void i8042_wait(void)
{
	uint8_t status;

	status = inportb(0x64);
	while ((status & 0x02) != 0)
		;
}

static void i8042_send_cmd(uint8_t cmd)
{
	i8042_wait();
	outportb(0x64, cmd);
}

static void i8042_send_data(uint8_t data)
{
	i8042_wait();
	outportb(0x60, data);
}

static uint8_t i8042_get_data(void)
{
	uint8_t status;

	status = inportb(0x64);
	while ((status & 0x01) == 0)
		;
	return inportb(0x60);
}

void a20_disable_i8042(void)
{
	uint8_t a;

	disable();

	/* Disable keyboard */
        i8042_send_cmd(0xAD);

	/* Read from port 2 */
	i8042_send_cmd(0xD0);
	a = i8042_get_data();

	/* Write port 2 */
	i8042_send_cmd(0xD1);
	i8042_send_data(a & ~0x02);

	/* Enable keyboard */
	i8042_send_cmd(0xAE);

	enable();
}


void a20_enable_i8042(void)
{
	uint8_t a;

	disable();

	/* Disable keyboard */
        i8042_send_cmd(0xAD);

	/* Read from port 2 */
	i8042_send_cmd(0xD0);
	a = i8042_get_data();

	/* Write port 2 */
	i8042_send_cmd(0xD1);
	i8042_send_data(a | 0x02);

	/* Enable keyboard */
	i8042_send_cmd(0xAE);

	enable();
}

/** Disable gate A20.
 *
 * @return Zero on succes, non-zero on error
 */
int a20_disable(void)
{
	int state;

	/* Check if A20 is already disabled */
	state = a20_check();
	if (state == 0)
		return 0;

	/* If not, try disabling using BIOS call */
	a20_disable_bios();

	/* Check again */
	state = a20_check();
	if (state == 0)
		return 0;

	/* If still not disabled, try disabling using i8042 */
	a20_disable_i8042();

	/* Check again */
	state = a20_check();
	if (state == 0)
		return 0;

	/* All attempts failed */
	return 1;
}

/** Enable gate A20.
 *
 * @return Zero on succes, non-zero on error
 */
int a20_enable(void)
{
	int state;

	/* Check if A20 is already enabled */
	state = a20_check();
	if (state != 0)
		return 0;

	/* If not, try enabling using BIOS call */
	a20_enable_bios();

	/* Check again */
	state = a20_check();
	if (state != 0)
		return 0;

	/* If still not enabled, try enabling using i8042 */
	a20_enable_i8042();

	/* Check again */
	state = a20_check();
	if (state != 0)
		return 0;

	/* All attempts failed */
	return 1;
}