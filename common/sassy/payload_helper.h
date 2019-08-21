#pragma once

#include <sassy/sassy.h>


#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define GET_LE_PAYLOAD(p, fld) (*(u32*)((p+offsetof(struct le_payload, fld))))

#define SET_LE_PAYLOAD(p, fld, v) do { \
  *((u32*)(((unsigned char *)p) + offsetof(struct le_payload, fld))) = v; \
} while (0)



