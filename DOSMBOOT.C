/*
 * Simplistic DOS application that loads and executes a Multiboot2-compliant
 * OS image.
 */
#include <dos.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
uint32_t gdt[4];

typedef struct {
	uint16_t limit;
	/* Note that base is not aligned, but BC will not try to
         * align it to 4 bytes */
	uint32_t base;
} pgdt_t;

pgdt_t pgdt;

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

	printf("header address: 0x%x\n", address.header_addr);
	printf("load address: 0x%x\n", address.load_addr);
	printf("load end address: 0x%x\n", address.load_end_addr);
	printf("bss end address: 0x%x\n", address.bss_end_addr);

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
		}

		if (rc != 0)
			return rc;
		mboot2_tag_end(f, &tag);
	}

	printf("Read multiboot header.\n");

	rewind(f);
	dbuf = malloc(16384);
	if (dbuf == NULL) {
		printf("Error allocating memory.\n");
		return -1;
	}

	daddr = address.load_addr;
	do {
		nr = fread(dbuf, 1, 16384, f);
		printf("read %u bytes at address 0x%lx\n", nr, daddr);
		daddr += nr;
	} while (nr == 16384);

	free(dbuf);
	fclose(f);

	return 0;
}

int main(int argc, char *argv[])
{
	printf("DOS MultiBoot2 loader.\n");
	(void) argc;
	(void) argv;

	if (protmode_is_prot() != 0) {
		printf("Cannot load OS while in protected mode!\n");
		return 1;
	}

	/* First GDT entry is unused */
	gdt[0] = 0;
	gdt[1] = 0;

	/* Second GDT entry is a 0-4G flat writable data segment
	/*
         * Base Address[15:0] = 0
         * Segment Limit[15:0] = 0xffff
	 */
	gdt[2] = 0x0000ffffL;
	/*
	 * Base Address[31:24] = 0
	 * G = 1 (4K granularity)
	 * D/B = 1
	 * AVL = 0 (not used)
	 * Segment Limit[19:16] = 0xf
	 * P = 1 (present)
	 * DPL = 0
	 * S = 1 (user segment)
	 * :11 = 0
	 * E = 0 (no expand down)
	 * W = 1 (writable)
	 * A = 0 (not accessed)
	 */
	gdt[3] = 0x00cf9200L;

	/* Two entries 8 bytes each */
	pgdt.limit = 2 * 8;
	pgdt.base = FP_SEG(&pgdt) * 16L + FP_OFF(&pgdt);
	printf("limit=0x%x base=0x%lx\n",
		pgdt.limit, pgdt.base);

	printf("sizeof(pgdt_t) = %u\n", sizeof(pgdt_t));
	printf("call protmode_loadhigh...\n");
	protmode_loadhigh(1024L*1024L + 65536L, argv, 10);
	printf("Done\n");
	return 0;

	//return mboot2_load("kernel.elf");
}
