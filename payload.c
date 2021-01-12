//
// Created by Riesop, Vincent on 09.12.20.
//

#include <stdlib.h>
#include <string.h>

#include "payload.h"
#include "logger.h"
#include "libasraft.h"
#include "consensus.h"
#include "ringbuffer.h"
#include "kvstore.h"
#include "replication.h"

void set_ae_data(unsigned char *pkt,
                 int32_t in_term,
                 int32_t in_leader_id,
                 int32_t first_idx,
                 int32_t in_prevLogTerm,
                 int32_t in_leaderCommitIdx,
                 struct consensus_priv *priv,
                 int32_t num_of_entries,
                 int more)
{
    struct sm_log_entry **entries = priv->sm_log.entries;
    uint16_t *opcode;
    int32_t *prev_log_idx, *leader_commit_idx;
    uint32_t *included_entries, *term, *prev_log_term, *leader_id;
    int i, buf_idx;
    uint32_t *cur_ptr;


    opcode = GET_CON_AE_OPCODE_PTR(pkt);
    *opcode = (uint16_t) APPEND;

    term = GET_CON_AE_TERM_PTR(pkt);
    *term = in_term;

    leader_id = GET_CON_AE_LEADER_ID_PTR(pkt);
    *leader_id = in_leader_id;

    prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(pkt);
    *prev_log_idx = first_idx - 1;

    prev_log_term = GET_CON_AE_PREV_LOG_TERM_PTR(pkt);
    *prev_log_term = in_prevLogTerm;

    included_entries = GET_CON_AE_NUM_ENTRIES_PTR(pkt);
    *included_entries = num_of_entries;

    leader_commit_idx = GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt);
    *leader_commit_idx = in_leaderCommitIdx;

    // done if there are no entries to include
    if (num_of_entries == 0)
        return;

    if (first_idx > priv->sm_log.last_idx) {
        asgard_error("Nothing to send, first_idx > priv->sm_log.last_idx %d, %d", first_idx, priv->sm_log.last_idx);
        *included_entries = 0;
        return;
    }

    //check if num_of_entries would exceed actual entries
    if ((first_idx + (num_of_entries - 1)) > priv->sm_log.last_idx) {
        asgard_error("BUG! can not send more entries than available... %d, %d, %d\n",
                     first_idx, num_of_entries, priv->sm_log.last_idx);
        *included_entries = 0;
        return;
    }

    cur_ptr = GET_CON_PROTO_ENTRIES_START_PTR(pkt);

    for (i = first_idx; i < first_idx + num_of_entries; i++) {

        /* Converting consensus idx to buffer idx */
        buf_idx = consensus_idx_to_buffer_idx(&priv->sm_log, i);

        if( buf_idx < 0 ){
            asgard_dbg("Could not translate idx=%d to buffer index ", i);
            return;
        }

        if (!entries[buf_idx]) {
            asgard_dbg("BUG! - entries at %d is null", i);
            return;
        }

        if (!entries[buf_idx]->dataChunk) {
            asgard_dbg("BUG! - entries dataChunk at %d is null", i);
            return;
        }

        //print_hex_dump(KERN_DEBUG, "writing to pkt: ", DUMP_PREFIX_NONE, 16,1,
        //              entries[buf_idx]->dataChunk->data, sizeof(struct data_chunk), 0);

        memcpy(cur_ptr, entries[buf_idx]->dataChunk->data, sizeof(struct data_chunk));

        // TODO: u32bit ptr increased twice to land at next data_chunk..
        //         ... however, we need a stable method if we change the data_chunk size!!
        cur_ptr++;
        cur_ptr++;


    }

    // asgard_dbg("Appended entries in next rep msg\n");


}



int setup_append_multicast_msg(struct asgard_device *sdev, struct asgard_payload *spay)
{
    int32_t local_last_idx;
    int32_t prev_log_term, leader_commit_idx;
    int32_t num_entries = 0;
    int32_t next_index;
    char *pkt_payload_sub;
    int more = 0;

    // Check if entries must be appended
    local_last_idx = get_last_idx_safe(sdev->consensus_priv);

    if (sdev->multicast.nextIdx == -1) {
        asgard_dbg("Invalid target id resulted in invalid next_index!\n");
        return -1;
    }

    // asgard_dbg("PREP AE: local_last_idx=%d, next_index=%d\n", local_last_idx, next_index);
    prev_log_term = get_prev_log_term(sdev->consensus_priv, sdev->multicast.nextIdx - 1);

    if (prev_log_term < 0) {
        asgard_error("BUG! - prev_log_term is invalid. next_index=%d\n", next_index);
        return -1;
    }

    pthread_mutex_lock(&sdev->consensus_priv->sm_log.next_lock);
    next_index = sdev->multicast.nextIdx;

    if (local_last_idx >= next_index) {
        // Facts:
        //	- local_last_idx >= next_index
        //  - Must include entries in next consensus append message
        //  - thus, num_of_entries will not be 0

        // Decide how many entries to update for the current target

        if (sdev->consensus_priv->max_entries_per_pkt < local_last_idx - next_index + 1) {
            num_entries = sdev->consensus_priv->max_entries_per_pkt;
            more = 1;
        } else {
            num_entries = (local_last_idx - next_index + 1);
            more = 0;
        }

        if(num_entries <= 0) {
            asgard_dbg("No entries to replicate\n");
            pthread_mutex_unlock(&sdev->consensus_priv->sm_log.next_lock);
            return -1;
        }

    } else {
        pthread_mutex_unlock(&sdev->consensus_priv->sm_log.next_lock);
        return -2;
    }

    /* asgard_dbg("retrans=%d, target_id=%d, leader_last_idx=%d, next_idx=%d, prev_log_term=%d, num_entries=%d\n",
                 retrans,
                 target_id,
                 local_last_idx,
                 next_index,
                 prev_log_term,
                 num_entries);*/

    // reserve space in asgard heartbeat for consensus LEAD

    pkt_payload_sub =
            asgard_reserve_proto(sdev->consensus_priv->ins->instance_id, spay,
                                 ASGARD_PROTO_CON_AE_BASE_SZ + (num_entries * AE_ENTRY_SIZE));

    if (!pkt_payload_sub) {
        pthread_mutex_unlock(&sdev->consensus_priv->sm_log.next_lock);
        return -1;
    }

    leader_commit_idx = next_index + num_entries - 1;

    set_ae_data(pkt_payload_sub,
                sdev->consensus_priv->term,
                sdev->consensus_priv->node_id,
            // previous is one before the "this should be send next" index
                next_index,
                prev_log_term,
                leader_commit_idx,
                sdev->consensus_priv,
                num_entries,
                more);

    sdev->multicast.nextIdx += num_entries;
    sdev->consensus_priv->sm_log.commit_idx += num_entries - 1;
    pthread_mutex_unlock(&sdev->consensus_priv->sm_log.next_lock);

    return more;
}



/* Protocol offsets and protocols_included must be correct before calling this method.
 *
 * Sets protocol id and reserves space in the asgard payload,
 * if the required space is available.
 *
 * returns a pointer to the start of that protocol payload memory area.
 */
char *asgard_reserve_proto(uint16_t instance_id, struct asgard_payload *spay, uint16_t proto_size)
{
    int i;
    char *cur_proto;
    int proto_offset = 0;
    int cur_offset = 0;

    uint16_t *pid, *poff;

    if(!spay){
        asgard_error("payload is NULL\n");
        return NULL;
    }

    cur_proto = spay->proto_data;

    // Check if protocol instance already exists in payload
    for (i = 0; i < spay->protocols_included; i++) {

        // if (instance_id == GET_PROTO_TYPE_VAL(cur_proto))
        // 	goto reuse; // reuse existing payload part for this instance id

        cur_offset = GET_PROTO_OFFSET_VAL(cur_proto);
        cur_proto = cur_proto + cur_offset;
        proto_offset += cur_offset;

    }
    spay->protocols_included++;

    if (proto_offset + proto_size > MAX_ASGARD_PAYLOAD_BYTES) {
        asgard_error("Not enough space in asgard payload. proto_offset: %d, protosize= %d\n", proto_offset, proto_size);
        spay->protocols_included--;
        return NULL;
    }

    pid =  GET_PROTO_TYPE_PTR(cur_proto);
    poff = GET_PROTO_OFFSET_PTR(cur_proto);

    *pid = instance_id;
    *poff = proto_size;


    return cur_proto;
}



int setup_append_msg(struct consensus_priv *cur_priv, struct asgard_payload *spay, int instance_id, int target_id, int32_t next_index, int retrans)
{
    int32_t local_last_idx;
    int32_t prev_log_term, leader_commit_idx;
    int32_t num_entries = 0;
    char *pkt_payload_sub;
    int more = 0;

    // Check if entries must be appended
    local_last_idx = get_last_idx_safe(cur_priv);

    /* Update next_index inside next_lock critical section */
    if(!retrans){
        pthread_mutex_lock(&cur_priv->sm_log.next_lock);
        next_index = get_next_idx(cur_priv, target_id);

        if(next_index == -2) {
            pthread_mutex_unlock(&cur_priv->sm_log.next_lock);
            return -2;
        }

    }


    if (next_index == -1) {
        asgard_dbg("Invalid target id resulted in invalid next_index!\n");
        if(!retrans)
            pthread_mutex_unlock(&cur_priv->sm_log.next_lock);
        return -1;
    }

    // asgard_dbg("PREP AE: local_last_idx=%d, next_index=%d\n", local_last_idx, next_index);
    prev_log_term = get_prev_log_term(cur_priv, next_index - 1);

    if (prev_log_term < 0) {
        asgard_error("BUG! - prev_log_term is invalid. next_index=%d, retrans=%d, target_id=%d\n", next_index, retrans, target_id );
        if(!retrans)
            pthread_mutex_unlock(&cur_priv->sm_log.next_lock);
        return -1;
    }

    leader_commit_idx = cur_priv->sm_log.commit_idx;

    if (local_last_idx >= next_index) {
        // Facts:
        //	- local_last_idx >= next_index
        //  - Must include entries in next consensus append message
        //  - thus, num_of_entries will not be 0

        // Decide how many entries to update for the current target

        if (cur_priv->max_entries_per_pkt < local_last_idx - next_index + 1) {
            num_entries = cur_priv->max_entries_per_pkt;
            more = 1;
        } else {
            num_entries = (local_last_idx - next_index + 1);
            more = 0;
        }

        if(num_entries <= 0) {
            asgard_dbg("No entries to replicate\n");
            if(!retrans)
                pthread_mutex_unlock(&cur_priv->sm_log.next_lock);
            return -1;
        }


        /* Update next_index without receiving the response from the target.
         *
         * If the receiver rejects this append command, this node will set
         * the next_index to the last known safe index of the receivers log.
         *
         * The receiver sends the last known safe index with the append reply.
         *
         * next_index must be read and increased in within critical section of next_lock!
         * next_index is only updated if this is not a retransmission!
         */
        if(!retrans)
            cur_priv->sm_log.next_index[target_id] += num_entries;

    } else {
        if(!retrans)
            pthread_mutex_unlock(&cur_priv->sm_log.next_lock);
        return -2;
    }

    if(!retrans)
        pthread_mutex_unlock(&cur_priv->sm_log.next_lock);

    /* asgard_dbg("retrans=%d, target_id=%d, leader_last_idx=%d, next_idx=%d, prev_log_term=%d, num_entries=%d\n",
                 retrans,
                 target_id,
                 local_last_idx,
                 next_index,
                 prev_log_term,
                 num_entries);*/

    // reserve space in asgard heartbeat for consensus LEAD
    pkt_payload_sub =
            asgard_reserve_proto(instance_id, spay,
                                 ASGARD_PROTO_CON_AE_BASE_SZ + (num_entries * AE_ENTRY_SIZE));

    if (!pkt_payload_sub) {
        return -1;
    }

    set_ae_data(pkt_payload_sub,
                cur_priv->term,
                cur_priv->node_id,
            // previous is one before the "this should be send next" index
                next_index,
                prev_log_term,
                leader_commit_idx,
                cur_priv,
                num_entries,
                more);

    return more;
}