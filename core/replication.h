#pragma once


#ifdef ASGARD_KERNEL_MODULE


#include <linux/ip.h>
#include <linux/udp.h>

#else
#include <sys/queue.h>
#endif




#include "libasraft.h"
#include "leader.h"
#include "payload.h"
#include "kvstore.h"
#include "pkthandler.h"


struct asgard_async_pkt {

    struct list_head async_pkts_head;

    struct asgard_packet_data pkt_data;

#ifdef ASGARD_KERNEL_MODULE
    struct sk_buff *skb;
#endif

};

struct retrans_request {
    int32_t request_idx;
    struct list_head retrans_req_head;
};

void async_clear_queue( struct asgard_async_queue_priv *queue);
int do_prepare_log_replication_multicast(struct asgard_device *sdev);
int do_prepare_log_replication(struct asgard_device *sdev, int target_id, int32_t next_index, int retrans);
int32_t get_next_idx(struct consensus_priv *priv, int target_id);
int32_t get_match_idx(struct consensus_priv *priv, int target_id);
void update_next_retransmission_request_idx(struct consensus_priv *priv);
void schedule_log_rep(struct asgard_device *sdev, int target_id, int next_index, int32_t retrans, int multicast_enabled);
void check_pending_log_rep(struct asgard_device *sdev);
void check_pending_log_rep_for_target(struct asgard_device *sdev, int target_id);