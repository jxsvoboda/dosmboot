/*
 * Simplistic DOS application that loads and executes a Multiboot2-compliant
 * OS image.
 */
#include <dos.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "a20.h"
#include "mboot2.h"
#include "protmode.h"

typedef struct {
	long start;
	long end;
	uint16_t type;
	uint16_t flags;
	uint32_t size;
} tag_t;

mboot2_tag_address_t address;
mboot2_tag_entry_t entry;
uint32_t gdt[10];

typedef struct {
	uint16_t limit;
	/* Note that base is not aligned, but BC will not try to
         * align it to 4 bytes */
	uint32_t base;
} pgdt_t;

/** GDT pointer */
pgdt_t pgdt;

/** Boot information structure */
uint32_t mbinfo[4];

static int mboot2_tag_start(FILE *f, uint32_t *remain, tag_t *tag)
{
	mboot2_tag_t taghdr;
	long end;
	size_t nr;

	tag->start = ftell(f);
	if (tag->start < 0)
		return -1;

	nr = fread(&taghdr, 1, sizeof(taghdr), f);
	if (nr < sizeof(taghdr))
		return -1;

	tag->type = taghdr.type;
	tag->flags = taghdr.flags;
	tag->size = taghdr.size;

	end = tag->start + tag->size;
	tag->end = (end + 7) - (end + 7) % 8;
	if (tag->end > tag->start + *remain)
		return -1;

	*remain -= (tag->end - tag->start);
	return 0;
}

static void mboot2_tag_end(FILE *f, tag_t *tag)
{
	(void) fseek(f, tag->end, SEEK_SET);
}

static int mboot2_tag_addr_read(FILE *f)
{
	size_t nr;

	printf("mboot2_tag_addr_read\n");
	nr = fread(&address, 1, sizeof(address), f);
	if (nr < sizeof(address))
		return -1;

	printf("header address: 0x%lx\n", address.header_addr);
	printf("load address: 0x%lx\n", address.load_addr);
	printf("load end address: 0x%lx\n", address.load_end_addr);
	printf("bss end address: 0x%lx\n", address.bss_end_addr);

	return 0;
}

static int mboot2_tag_entry_read(FILE *f)
{
	size_t nr;

	printf("mboot2_tag_addr_read\n");
	nr = fread(&entry, 1, sizeof(entry), f);
	if (nr < sizeof(entry))
		return -1;

	printf("entry address: 0x%lx\n", entry.entry_addr);
	return 0;
}


int mboot2_load(const char *fname)
{
	FILE *f;
	uint8_t buf[MBOOT2_HDR_ALIGN];
	uint8_t *dbuf;
	mboot2_hdr_t hdr;
	tag_t tag;
	size_t nr;
	uint32_t remain;
	uint32_t daddr;
	uint32_t mbptr;
	long hdr_off;
	long load_off;
	int rc;

	f = fopen(fname, "rb");
	if (f == NULL) {
		printf("Error opening '%s'.\n", fname);
		return -1;
	}

	while (true) {
		nr = fread(buf, 1, sizeof(buf), f);
		if (ferror(f)) {
			printf("Error reading '%s'.\n", fname);
			return -1;
		}

		if (feof(f) || nr < sizeof(buf)) {
			printf("Unexpected end of file reading '%s'.\n", fname);
			return -1;
		}

		if (*(uint32_t *)buf == MBOOT2_MAGIC)
			break;
	}

	printf("Header magic value found.\n");
	if (fseek(f, -MBOOT2_HDR_ALIGN, SEEK_CUR) < 0) {
		printf("Error seeking.\n");
		return -1;
	}

	/* Offset at which multiboot header starts */
	hdr_off = ftell(f);
	if (hdr_off < 0) {
		printf("Error determining file position.\n");
		return -1;
	}

	nr = fread(&hdr, 1, sizeof(hdr), f);
	if (nr != sizeof(hdr)) {
		printf("Error reading multiboot header.\n");
		return -1;
	}

	printf("Header length: %u\n", hdr.header_length);
	remain = hdr.header_length - sizeof(hdr);
	printf("remain=%d\n", remain);

	while (remain > 0) {
		rc = mboot2_tag_start(f, &remain, &tag);
		if (rc < 0) {
			printf("Erro reading multiboot header.\n");
			return -1;
		}

		printf("tag.type = %d\n", tag.type);
		rc = 0;
		switch (tag.type) {
		case mboot2_tag_address:
			rc = mboot2_tag_addr_read(f);
			break;
		case mboot2_tag_entry:
			rc = mboot2_tag_entry_read(f);
			break;
		}

		if (rc != 0)
			return rc;
		mboot2_tag_end(f, &tag);
	}

	printf("Read multiboot header (offset=0x%lx).\n", hdr_off);

	load_off = hdr_off - (address.header_addr - address.load_addr);
	printf("Load start offset 0x%lx\n", load_off);

	rc = fseek(f, load_off, SEEK_SET);
	if (rc != 0) {
		printf("Error seeking.\n");
		return  -1;
	}

	dbuf = malloc(16384);
	if (dbuf == NULL) {
		printf("Error allocating memory.\n");
		return -1;
	}

	daddr = address.load_addr;
	//daddr = FP_SEG(dbuf) * 16 + FP_OFF(dbuf); // XXX
	do {
		nr = fread(dbuf, 1, 16384, f);
		//delay(1000);
//		printf("read %u bytes at address 0x%lx from dbuf=%x\n", nr, daddr, dbuf);
		protmode_loadhigh(daddr, dbuf, nr);
		daddr += nr;
	} while (nr == 16384);

	free(dbuf);
	fclose(f);

	printf("OS image loaded.\n");

	mbptr = FP_SEG(mbinfo) * 16L + FP_OFF(mbinfo);
	mbinfo[0] = 16;
	mbinfo[1] = 0;
	mbinfo[2] = 0;
	mbinfo[3] = 8;

	printf("MBinfo=0x%lx, start addr = 0x%lx\n", mbptr, entry.entry_addr);
	printf("Sleep before starting kernel...\n");
	delay(5000);
	//printf("Start kernel...\n");
	protmode_os_exec(mbptr, entry.entry_addr);

	return 0;
}

int main(int argc, char *argv[])
{
	uint16_t code_seg;
	uint32_t code_seg_ba;
	uint16_t data_seg;
	uint32_t data_seg_ba;
	int a20;
	int rc;

	printf("DOS MultiBoot2 loader.\n");
	(void) argc;
	(void) argv;

	code_seg = FP_SEG(main);
	code_seg_ba = (uint32_t)code_seg << 4;

	data_seg = FP_SEG(gdt);
	data_seg_ba = (uint32_t)data_seg << 4;

/*	printf("code_seg=0x%x data_seg=0x%x\n", code_seg, data_seg);
	printf("code_seg_ba=0x%lx data_seg_ba=0x%lx\n",
	    code_seg_ba, data_seg_ba);
	printf("main=%04x:%04x\n",
		FP_SEG(main), FP_OFF(main));*/
	printf("protmode_loadhigh=%04x:%04x\n",
		FP_SEG(protmode_loadhigh), FP_OFF(protmode_loadhigh));
	printf("protmode_os_exec=%04x:%04x\n",
		FP_SEG(protmode_os_exec), FP_OFF(protmode_os_exec));

	if (protmode_is_prot() != 0) {
		printf("Cannot load OS while in protected mode!\n");
		return 1;
	}

	a20 = a20_check();
	if (a20) {
		printf("Gate A20 is already enabled.\n");
	} else {
		if (a20_enable() != 0) {
			printf("Error enabling gate A20!\n");
			return 1;
		}

		printf("Enabled gate A20.\n");
	}

	/* First GDT entry is unused */
	gdt[0] = 0;
	gdt[1] = 0;

	/* Second GDT entry is a 16-bit code segment
	/*
         * Base Address[15:0] = ?
         * Segment Limit[15:0] = 0xffff
	 */
	gdt[2] = 0x0000ffffL | ((code_seg_ba & 0xffffL) << 16);
	/*
	 * Base Address[31:24] = ?
	 * G = 0 (1B granularity)
	 * D = 0 (default operand size: 16 bits)
	 * AVL = 0 (not used)
	 * Segment Limit[19:16] = 0x0
	 * P = 1 (present)
	 * DPL = 0
	 * S = 1 (user segment)
	 * :11 = 1 (code)
	 * E = 0 (no expand down)
	 * R = 1 (readable)
	 * A = 0 (not accessed)
	 * Base Address[23:16] = ?
	 */
	gdt[3] = 0x00009a00L | (code_seg_ba & 0xff000000L) |
	    ((code_seg_ba >> 16) & 0xff);

	/* Third GDT entry is a 16-bit data segment
	/*
         * Base Address[15:0] = ?
         * Segment Limit[15:0] = 0xffff
	 */
	gdt[4] = 0x0000ffffL | ((data_seg_ba & 0xffffL) << 16);
	/*
	 * Base Address[31:24] = ?
	 * G = 0 (1B granularity)
	 * D = 0 (default operand size: 16 bits)
	 * AVL = 0 (not used)
	 * Segment Limit[19:16] = 0x0
	 * P = 1 (present)
	 * DPL = 00
	 * S = 1 (user segment)
	 * :11 = 0 (data)
	 * E = 0 (no expand down)
	 * W = 1 (writable)
	 * A = 0 (not accessed)
	 * Base Address[23:16] = ?
	 */
	gdt[5] = 0x00009200L | (data_seg_ba & 0xff000000L) |
	    ((data_seg_ba >> 16) & 0xff);

	/* Fourth GDT entry is a 0-4G flat writable data segment
	/*
         * Base Address[15:0] = 0
         * Segment Limit[15:0] = 0xffff
	 */
	gdt[6] = 0x0000ffffL;
	/*
	 * Base Address[31:24] = 0
	 * G = 1 (4K granularity)
	 * D/B = 1
	 * AVL = 0 (not used)
	 * Segment Limit[19:16] = 0xf
	 * P = 1 (present)
	 * DPL = 0
	 * S = 1 (user segment)
	 * :11 = 0 (data)
	 * E = 0 (no expand down)
	 * W = 1 (writable)
	 * A = 0 (not accessed)
	 * Base Address[23:16] = 0
	 */
	gdt[7] = 0x00cf9200L;

	/* Fifth GDT entry is a 0-4G flat code segment
	/*
         * Base Address[15:0] = 0
         * Segment Limit[15:0] = 0xffff
	 */
	gdt[8] = 0x0000ffffL;
	/*
	 * Base Address[31:24] = 0
	 * G = 1 (4K granularity)
	 * D = 0 (default operand size: 16 bits)
	 * AVL = 0 (not used)
	 * Segment Limit[19:16] = 0x0
	 * P = 1 (present)
	 * DPL = 0
	 * S = 1 (user segment)
	 * :11 = 1 (code)
	 * E = 0 (no expand down)
	 * R = 1 (readable)
	 * A = 0 (not accessed)
	 * Base Address[23:16] = 0
	 */
	gdt[9] = 0x00cf9a00L;

	/* Four entries 8 bytes each */
	pgdt.limit = 4 * 10;
	pgdt.base = FP_SEG(gdt) * 16L + FP_OFF(gdt);
//	printf("limit=0x%x base=0x%lx\n",
//		pgdt.limit, pgdt.base);

//	printf("sizeof(pgdt_t) = %u\n", sizeof(pgdt_t));
	//printf("call protmode_loadhigh...\n");
	//protmode_loadhigh(1024L*1024L + 65536L, argv, 10);
	//printf("Done\n");
//	return 0;

	printf("Sleep before loading kernel...\n");
	delay(5000);
	printf("Load kernel...\n");
	rc = mboot2_load("kernel.elf");

	if (a20) {
		printf("Left gate A20 enabled.\n");
	} else {
		if (a20_disable() != 0) {
			printf("Error disabling gate A20!\n");
			return 1;
		}

		printf("Disabled gate A20.\n");
	}

	if (rc != 0)
		return 1;
        return 0;
}
