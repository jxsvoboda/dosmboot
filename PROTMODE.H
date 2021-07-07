#ifndef PROTMODE_H
#define PROTMODE_H

#include <stddef.h>
#include <stdint.h>

extern int protmode_is_prot(void);
extern void protmode_loadhigh(uint32_t, const void *src, size_t n);
extern void protmode_os_exec(uint32_t, uint32_t);

#endif