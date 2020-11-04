#ifndef _ASGARD_PHELP_H_
#define _ASGARD_PHELP_H_

#include <asgard/asgard.h>
#include <asgard/logger.h>
#include <asgard/consensus.h>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

// #define GET_LE_PAYLOAD(p, fld) (*(u32*)((p + offsetof(struct le_payload, fld))))

/* #define SET_LE_PAYLOAD(p, fld, v) do { \
   *((u32*)(((unsigned char *)p) + offsetof(struct le_payload, fld))) = v; \
 } while (0)
 */

/* TODO: Find a method to define the byte layout via structs and get the memory offsets via
 * a similiar method like the GET_LE_PAYLOAD from above.
 *
 * The task is to introduce another layer of indirection to find out the ptr size for the
 * corresponding struct field..
 *
 * These macros are cumbersome to maintain, but as always...
 * "meeting the deadline" >>> maintainability
 *
 * Sorry!
 */

// protoid(u16) + offset(u16)
#define GET_CON_AE_OPCODE_PTR(p) (u16 *)((char *) p + 2 + 2)

// protoid(u16) + offset(u16) + opcode(u16)
#define GET_CON_AE_TERM_PTR(p) (u32 *)((char *) p + 4 + 2)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32)
#define GET_CON_AE_LEADER_ID_PTR(p) (u32 *)((char *) p + 6 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32)
#define GET_CON_AE_PREV_LOG_IDX_PTR(p) (s32 *)((char *) p + 10 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32) + prev_log_idx(u32)
#define GET_CON_AE_PREV_LOG_TERM_PTR(p) (u32 *)((char *) p + 14 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32) + prev_log_idx(u32) + prev_term(u32)
#define GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(p) (s32 *)((char *) p + 18 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32) + prev_log_idx(u32) + prev_term(u32)
// .... + leader_commit_idx(32)
#define GET_CON_AE_NUM_ENTRIES_PTR(p) (u32 *)((char *) p + 22 + 4)
#define GET_CON_AE_NUM_ENTRIES_VAL(p) (*(u32 *)((char *) p + 22 + 4))

// protoid(u16) + offset(u16) + opcode(u16) + term(u32) + leader_id(u32) + prev_log_idx(u32) + prev_term(u32)
// .... + leader_commit_idx(32) + num_entries(u32)
#define GET_CON_PROTO_ENTRIES_START_PTR(p) (u32 *)((char *) p + 26 + 4)


// protoid(u16) + offset(u16)
#define GET_CON_PROTO_OPCODE_VAL(p) (*(u16 *)((char *) p + 2 + 2))
#define GET_CON_PROTO_OPCODE_PTR(p) (u16 *)((char *) p + 2 + 2)

// protoid(u16) + offset(u16) + opcode(u16)
#define GET_CON_PROTO_PARAM1_VAL(p) (*(u32 *)((char *) p + 2 + 2 + 2))
#define GET_CON_PROTO_PARAM1_PTR(p) (u32 *)((char *) p + 2 + 2 + 2)

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32)
#define GET_CON_PROTO_PARAM2_VAL(p) (*(s32 *)((char *) p + 2 + 2 + 2 + 4))
#define GET_CON_PROTO_PARAM2_PTR(p) (s32 *)((char *) p + 2 + 2 + 2 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32) + param2(u32)
#define GET_CON_PROTO_PARAM3_VAL(p) (*(s32 *)((char *) p + 2 + 2 + 2 + 4 + 4))
#define GET_CON_PROTO_PARAM3_PTR(p) (s32 *)((char *) p + 2 + 2 + 2 + 4 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32) + param2(u32)
//#define GET_CON_PROTO_PARAM3_MAC_VAL(p) (*(char *)((char *) p + 2 + 2 + 2 + 4 + 4))
#define GET_CON_PROTO_PARAM3_MAC_PTR(p) (char *)((char *) p + 2 + 2 + 2 + 4 + 4)

// protoid(u16) + offset(u16) + opcode(u16) + param1(u32) + param2(u32)  + param3(u32)
#define GET_CON_PROTO_PARAM4_VAL(p) (*(s32 *)((char *) p + 2 + 2 + 2 + 4 + 4 + 4))
#define GET_CON_PROTO_PARAM4_PTR(p) (s32 *)((char *) p + 2 + 2 + 2 + 4 + 4 + 4)

#define GET_PROTO_AMOUNT_VAL(p) (*(u16 *)((char *) p))
#define GET_PROTO_AMOUNT_PTR(p) (u16 *)((char *) p)

#define GET_PROTO_START_SUBS_PTR(p) (char *)((char *)p + 2)


#define GET_PROTO_TYPE_VAL(p) (*(u16 *)((char *)p))
#define GET_PROTO_TYPE_PTR(p) (u16 *)((char *)p)


#define GET_PROTO_OFFSET_VAL(p) (*(u16 *)(((char *)p) + 2))
#define GET_PROTO_OFFSET_PTR(p) (u16 *)(((char *)p) + 2)



/*  Payload Offset BYte Helper for Echo Protocol  */

#define GET_ECHO_PROTO_OPCODE_VAL(p) (*(u16 *)((char *) p + 2 + 2))
#define GET_ECHO_PROTO_OPCODE_PTR(p) (u16 *)((char *) p + 2 + 2)

#define GET_ECHO_PROTO_SENDER_ID_VAL(p) (*(u32 *)((char *) p + 2 + 2 + 2))
#define GET_ECHO_PROTO_SENDER_ID_PTR(p) (u32 *)((char *) p + 2 + 2 + 2)

#define GET_ECHO_PROTO_RECEIVER_ID_VAL(p) (*(u32 *)((char *) p + 2 + 2 + 2 + 4))
#define GET_ECHO_PROTO_RECEIVER_ID_PTR(p) (u32 *)((char *) p + 2 + 2 + 2 + 4)

#define GET_ECHO_PROTO_TS1_VAL(p) (*(s64 *)((char *) p + 2 + 2 + 2 + 4 + 4))
#define GET_ECHO_PROTO_TS1_PTR(p) (s64 *)((char *) p + 2 + 2 + 2 + 4 + 4)

#define GET_ECHO_PROTO_TS2_VAL(p) (*(s64 *)((char *) p + 2 + 2 + 2 + 4 + 4 + 8))
#define GET_ECHO_PROTO_TS2_PTR(p) (s64 *)((char *) p + 2 + 2 + 2 + 4 + 4 + 8)

#define GET_ECHO_PROTO_TS3_VAL(p) (*(s64 *)((char *) p + 2 + 2 + 2 + 4 + 4 + 8 + 8))
#define GET_ECHO_PROTO_TS3_PTR(p) (s64 *)((char *) p + 2 + 2 + 2 + 4 + 4 + 8 + 8)



typedef int(*handle_payload_fun)(struct asgard_device *,
								 unsigned char *remote_mac,
								 void *payload);

// TODO: why 20?? Why not 22?
#define ASGARD_PROTO_CON_AE_BASE_SZ 20
#define CONLOG_ENTRY_SIZE 8

// 2 isntance ID + 2 proto offset + 2 opcode + 4 param1 + 4 param2 + 4 param3 + 4 param4
#define ASGARD_PROTO_CON_PAYLOAD_SZ 22
#define ASGARD_CON_METADATA_SZ 23
#define ASGARD_CON_LOGCMD_SZ 8

// TODO: correct payload sizes!!
#define ASGARD_PROTO_FD_PAYLOAD_SZ 2
// 34 Bytes for opcode, id, id,ts,ts,ts + 2 instance id, 2 proto offset
#define ASGARD_PROTO_ECHO_PAYLOAD_SZ 38


int get_proto_offset(char *cur);
char *asgard_get_proto(struct asgard_payload *spay, int n);
char *asgard_reserve_proto(u16 instance_id, struct asgard_payload *spay, u16 proto_size);
void invalidate_proto_data(struct asgard_device *sdev, struct asgard_payload *spay);
int setup_append_msg(struct consensus_priv *cur_priv, struct asgard_payload *spay, int instance_id, int target_id, s32 next_index, int retrans);
int setup_alive_msg(struct consensus_priv *cur_priv, struct asgard_payload *spay, int instance_id);
int setup_cluster_join_advertisement(struct asgard_payload *spay, int advertise_id, u32 ip, unsigned char *mac);
void check_pending_log_rep(struct asgard_device *sdev);
s32 _get_match_idx(struct consensus_priv *priv, int target_id);
void check_pending_log_rep_for_target(struct asgard_device *sdev, int target_id);

#endif  /* _ASGARD_PHELP_H_ */
