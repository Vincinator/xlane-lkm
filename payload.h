#pragma once

#include <stdint.h>

#include "libasraft.h"


// protoid(u16) + offset(u16)
#define GET_CON_AE_OPCODE_PTR(p) (uint16_t *)((char *) p + 2 + 2)

// protoid(u16) + offset(u16) + opcode(u16)
#define GET_CON_AE_TERM_PTR(p) (uint32_t *)((char *) p + 4 + 2)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32)
#define GET_CON_AE_LEADER_ID_PTR(p) (uint32_t *)((char *) p + 6 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32)
#define GET_CON_AE_PREV_LOG_IDX_PTR(p) (int32_t *)((char *) p + 10 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32) + prev_log_idx(u32)
#define GET_CON_AE_PREV_LOG_TERM_PTR(p) (uint32_t *)((char *) p + 14 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32) + prev_log_idx(u32) + prev_term(u32)
#define GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(p) (int32_t *)((char *) p + 18 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32) + prev_log_idx(u32) + prev_term(u32)
// .... + leader_commit_idx(32)
#define GET_CON_AE_NUM_ENTRIES_PTR(p) (uint32_t *)((char *) p + 22 + 4)
#define GET_CON_AE_NUM_ENTRIES_VAL(p) (*(uint32_t *)((char *) p + 22 + 4))

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32) + prev_log_idx(u32) + prev_term(u32)
// .... + leader_commit_idx(32) + num_entries(u32)
#define GET_CON_PROTO_ENTRIES_START_PTR(p) (uint32_t *)((char *) p + 26 + 4)


// protoid(u16) + offset(u16)
#define GET_CON_PROTO_OPCODE_VAL(p) (*(uint16_t *)((char *) p + 2 + 2))
#define GET_CON_PROTO_OPCODE_PTR(p) (uint16_t *)((char *) p + 2 + 2)

// protoid(u16) + offset(u16) + opcode(u16)
#define GET_CON_PROTO_PARAM1_VAL(p) (*(uint32_t *)((char *) p + 2 + 2 + 2))
#define GET_CON_PROTO_PARAM1_PTR(p) (uint32_t *)((char *) p + 2 + 2 + 2)

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32)
#define GET_CON_PROTO_PARAM2_VAL(p) (*(int32_t *)((char *) p + 2 + 2 + 2 + 4))
#define GET_CON_PROTO_PARAM2_PTR(p) (int32_t *)((char *) p + 2 + 2 + 2 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32) + param2(u32)
#define GET_CON_PROTO_PARAM3_VAL(p) (*(int32_t *)((char *) p + 2 + 2 + 2 + 4 + 4))
#define GET_CON_PROTO_PARAM3_PTR(p) (int32_t *)((char *) p + 2 + 2 + 2 + 4 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32) + param2(u32)
//#define GET_CON_PROTO_PARAM3_MAC_VAL(p) (*(char *)((char *) p + 2 + 2 + 2 + 4 + 4))
#define GET_CON_PROTO_PARAM3_MAC_PTR(p) (char *)((char *) p + 2 + 2 + 2 + 4 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32) + param2(u32)  + param3(u32)
#define GET_CON_PROTO_PARAM4_VAL(p) (*(int32_t *)((char *) p + 2 + 2 + 2 + 4 + 4 + 4))
#define GET_CON_PROTO_PARAM4_PTR(p) (int32_t *)((char *) p + 2 + 2 + 2 + 4 + 4 + 4)

#define GET_PROTO_AMOUNT_VAL(p) (*(uint16_t *)((char *) p))
#define GET_PROTO_AMOUNT_PTR(p) (uint16_t *)((char *) p)

#define GET_PROTO_START_SUBS_PTR(p) (char *)((char *)p + 2)


#define GET_PROTO_TYPE_VAL(p) (*(uint16_t *)((char *)p))
#define GET_PROTO_TYPE_PTR(p) (uint16_t *)((char *)p)


#define GET_PROTO_OFFSET_VAL(p) (*(uint16_t *)(((char *)p) + 2))
#define GET_PROTO_OFFSET_PTR(p) (uint16_t *)(((char *)p) + 2)
int setup_append_msg(struct consensus_priv *cur_priv, struct asgard_payload *spay, int instance_id, int target_id, int32_t next_index, int retrans);

unsigned char * asgard_reserve_proto(uint16_t instance_id, struct asgard_payload *spay, uint16_t proto_size);