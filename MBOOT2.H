#ifndef MBOOT2_H
#define MBOOT2_H

/*
 * Multiboot2 definitions
 */

#include <stdint.h>

/** Multiboot2 header */
typedef struct {
	uint32_t magic;
	uint32_t architecture;
	uint32_t header_length;
	uint32_t checksum;
} mboot2_hdr_t;

#define MBOOT2_MAGIC 0xE85250D6UL
#define MBOOT2_ARCH_i386 0
/** The header must be completely contained within the first 32K of the image */
#define MBOOT2_HDR_LIMIT 32768
#define MBOOT2_HDR_ALIGN 8

enum {
	mboot2_tag_address = 2,
	mboot2_tag_entry = 3
};

/** Multiboot2 general tag header */
typedef struct {
	uint16_t type;
	uint16_t flags;
	uint32_t size;
} mboot2_tag_t;

typedef struct {
	uint32_t header_addr;
	uint32_t load_addr;
	uint32_t load_end_addr;
	uint32_t bss_end_addr;
} mboot2_tag_address_t;

typedef struct {
	uint32_t entry_addr;
} mboot2_tag_entry_t;

#endif
