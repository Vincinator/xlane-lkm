#pragma once

#include <sassy/sassy.h>


#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define GET_LE_PAYLOAD(p, fld) (*(u32*)((p + offsetof(struct le_payload, fld))))

#define SET_LE_PAYLOAD(p, fld, v) do { \
  *((u32*)(((unsigned char *)p) + offsetof(struct le_payload, fld))) = v; \
} while (0)


#define SASSY_PROTO_CON_PAYLOAD_SZ 9
#define SASSY_CON_METADATA_SZ 23
#define SASSY_CON_LOGCMD_SZ 8

// TODO: correct payload sizes!!
#define SASSY_PROTO_FD_PAYLOAD_SZ 2
#define SASSY_PROTO_ECHO_PAYLOAD_SZ 2


int get_proto_offset(char *cur);
char *sassy_get_proto(struct sassy_payload *spay, int n);
char *sassy_reserve_proto(struct sassy_payload *spay, u16 proto_size, u16 proto_id);


