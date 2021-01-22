#ifndef LIBASRAFT_CONSENSUS_H
#define LIBASRAFT_CONSENSUS_H



#ifndef ASGARD_KERNEL_MODULE
#include <pthread.h>
#endif

#include "types.h"
#include "libasraft.h"

#include "logger.h"
#ifdef ASGARD_KERNEL_MODULE
#include "list.h"
#endif


#define MAX_CONSENSUS_LOG 400000
#define AE_ENTRY_SIZE 8
#define ASGARD_PROTO_CON_AE_BASE_SZ 20
#define MAX_AE_ENTRIES_PER_PKT 100

#define MIN_FTIMEOUT_NS 10000000
#define MAX_FTIMEOUT_NS 20000000
#define MIN_CTIMEOUT_NS 20000000
#define MAX_CTIMEOUT_NS 40000000



#define ASGARD_PROTO_CON_PAYLOAD_SZ 22
#define ASGARD_CON_METADATA_SZ 23
#define ASGARD_CON_LOGCMD_SZ 8

void log_le_rx(enum node_state nstate, uint64_t ts, int term, enum le_opcode opcode, int rcluster_id, int rterm);
const char *nstate_string(enum node_state state);
int node_transition(struct proto_instance *ins, node_state_t state);
char *le_state_name(enum le_state state);
void set_le_opcode(unsigned char *pkt, enum le_opcode opco, int32_t p1, uint32_t p2, int32_t p3, int32_t p4);
int setup_le_msg(struct proto_instance *ins, struct pminfo *spminfo, enum le_opcode opcode,
                 int32_t target_id, int32_t param1, int32_t param2, int32_t param3, int32_t param4);
void accept_leader(struct proto_instance *ins, int remote_lid, int cluster_id, int32_t term);
int check_handle_nomination(struct consensus_priv *priv, uint32_t param1, uint32_t param3, uint32_t param4,
                            int rcluster_id);

int setup_le_broadcast_msg(struct proto_instance *ins, enum le_opcode opcode);
void _handle_append_rpc(struct proto_instance *ins, struct consensus_priv *priv, unsigned char *pkt,  int remote_lid, int rcluster_id);
void reply_vote(struct proto_instance *ins, int remote_lid, int rcluster_id, int32_t param1, int32_t param2);

void reply_append(struct proto_instance *ins, struct pminfo *spminfo, int remote_lid, int param1, int append_success,
                  uint32_t logged_idx);

void check_pending_log_rep_for_multicast(struct asgard_device *sdev);

struct proto_instance *get_consensus_proto_instance(struct asgard_device *sdev);
#endif //LIBASRAFT_CONSENSUS_H
