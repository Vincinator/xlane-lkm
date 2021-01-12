#pragma once

#include <sys/queue.h>
#include "libasraft.h"




struct asgard_async_pkt {

    struct list_head async_pkts_head;

    struct asgard_packet_data pkt_data;
};

struct retrans_request {
    int32_t request_idx;
    struct list_head retrans_req_head;
};

int32_t get_next_idx(struct consensus_priv *priv, int target_id);
int32_t get_match_idx(struct consensus_priv *priv, int target_id);
void update_next_retransmission_request_idx(struct consensus_priv *priv);
void schedule_log_rep(struct asgard_device *sdev, int target_id, int next_index, int32_t retrans, int multicast_enabled);
void check_pending_log_rep(struct asgard_device *sdev);
void check_pending_log_rep_for_target(struct asgard_device *sdev, int target_id);