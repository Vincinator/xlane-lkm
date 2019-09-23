#pragma once

#include <sassy/sassy.h>


#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define GET_LE_PAYLOAD(p, fld) (*(u32*)((p + offsetof(struct le_payload, fld))))

#define SET_LE_PAYLOAD(p, fld, v) do { \
  *((u32*)(((unsigned char *)p) + offsetof(struct le_payload, fld))) = v; \
} while (0)

// protoid(u16) + offset(u16)
#define GET_CON_PROTO_OPCODE_VAL(p) *(u16 *)((char*) p + 2 + 2)
#define GET_CON_PROTO_OPCODE_PTR(p) (u16 *)((char*) p + 2 + 2)

// protoid(u16) + offset(u16) + opcode(u16)
#define GET_CON_PROTO_PARAM1_VAL(p) *(u32 *)((char*) p + 2 + 2 + 2)
#define GET_CON_PROTO_PARAM1_PTR(p) (u32 *)((char*) p + 2 + 2 + 2 )

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32)
#define GET_CON_PROTO_PARAM2_VAL(p) *(u32 *)((char*) p + 2 + 2 + 2 + 4)
#define GET_CON_PROTO_PARAM2_PTR(p) (u32 *)((char*) p + 2 + 2 + 2 + 4)

#define GET_PROTO_AMOUNT_VAL(p) *(u16 *)((char*) p)
#define GET_PROTO_AMOUNT_PTR(p) (u16 *)((char*) p)

#define GET_PROTO_START_SUBS_PTR(p) (char *)((char *)p + 2)


#define GET_PROTO_TYPE_VAL(p) *(u16 *)((char*)p)
#define GET_PROTO_TYPE_PTR(p) (u16 *)((char*)p)


#define GET_PROTO_OFFSET_VAL(p) *(u16 *)(((char*)p) + 2)
#define GET_PROTO_OFFSET_PTR(p) (u16 *)(((char*)p) + 2)


typedef int(*handle_payload_fun)(struct sassy_device *,
								 unsigned char *remote_mac,
								 void *payload);

#define SASSY_PROTO_CON_PAYLOAD_SZ 9
#define SASSY_CON_METADATA_SZ 23
#define SASSY_CON_LOGCMD_SZ 8

// TODO: correct payload sizes!!
#define SASSY_PROTO_FD_PAYLOAD_SZ 2
#define SASSY_PROTO_ECHO_PAYLOAD_SZ 2


int get_proto_offset(char *cur);
char *sassy_get_proto(struct sassy_payload *spay, int n);
char *sassy_reserve_proto(u16 instance_id, struct sassy_payload *spay, u16 proto_size);
void invalidate_proto_data(struct sassy_device *sdev, struct sassy_payload *spay);

handle_payload_fun get_payload_handler(enum sassy_protocol_type protocol_id);


