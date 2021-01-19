#include <errno.h>
#include <unistd.h>

#include "consensus.h"
#include "membership.h"
#include "payload.h"
#include "kvstore.h"
#include "pacemaker.h"
#include "ringbuffer.h"
#include "libasraft.h"
#include "follower.h"
#include "candidate.h"
#include "leader.h"
#include "replication.h"
#include "logger.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][CONSENSUS]"


const char *nstate_string(enum node_state state)
{
    switch (state) {
        case FOLLOWER:
            return "Follower";
        case CANDIDATE:
            return "Candidate";
        case LEADER:
            return "Leader";
        default:
            return "Unknown State ";
    }
}

char *le_state_name(enum le_state state) {
    switch (state) {
        case LE_RUNNING:
            return "RUNNING";
        case LE_READY:
            return "READY";
        case LE_UNINIT:
            return "UNINIT";
        default:
            return "UNKNOWN STATE";
    }
}

const char *opcode_string(enum le_opcode opcode)
{
    switch (opcode) {
        case VOTE:
            return "Vote";
        case NOMI:
            return "Nomination";
        case NOOP:
            return "Noop";
        case APPEND:
            return "Append";
        case APPEND_REPLY:
            return "Append Reply";
        case ALIVE:
            return "Alive";
        default:
            return "Unknown State ";
    }
}

void log_le_rx(enum node_state nstate, uint64_t ts, int term, enum le_opcode opcode, int rcluster_id, int rterm)
{
    if (opcode == NOOP)
        return;

    if (opcode == ALIVE)
        return;

    if (opcode == ADVERTISE)
        return;

    asgard_log_le("%s, %lu, %d: %s from %d with term %d\n",
                  nstate_string(nstate),
                  ts,
                  term,
                  opcode_string(opcode),
                  rcluster_id,
                  rterm);
}

void set_le_opcode(unsigned char *pkt, enum le_opcode opco, int32_t p1, uint32_t p2, int32_t p3, int32_t p4) {
    uint16_t *opcode;
    uint32_t *param1;
    int32_t *param2, *param3, *param4;

    opcode = GET_CON_PROTO_OPCODE_PTR(pkt);
    *opcode = (uint16_t) opco;

    param1 = GET_CON_PROTO_PARAM1_PTR(pkt);
    *param1 = (uint32_t) p1;

    param2 = GET_CON_PROTO_PARAM2_PTR(pkt);
    *param2 = (int32_t) p2;

    param3 = GET_CON_PROTO_PARAM3_PTR(pkt);
    *param3 = (int32_t) p3;

    param4 = GET_CON_PROTO_PARAM4_PTR(pkt);
    *param4 = (int32_t) p4;
}

int setup_le_msg(struct proto_instance *ins, struct pminfo *spminfo, enum le_opcode opcode,
                 int32_t target_id, int32_t param1, int32_t param2, int32_t param3, int32_t param4) {
    struct asgard_payload *pkt_payload;
    unsigned char *pkt_payload_sub;

    pthread_mutex_lock(&spminfo->pm_targets[target_id].pkt_data.mlock);

    pkt_payload =
            spminfo->pm_targets[target_id].pkt_data.payload;

    pkt_payload_sub =
            asgard_reserve_proto(ins->instance_id, pkt_payload, ASGARD_PROTO_CON_PAYLOAD_SZ);

    if (!pkt_payload_sub) {
        asgard_error("Leader Election packet full!\n");
        goto unlock;
    }

    set_le_opcode((unsigned char *) pkt_payload_sub, opcode, param1, param2, param3, param4);

unlock:
    pthread_mutex_unlock(&spminfo->pm_targets[target_id].pkt_data.mlock);
    return 0;
}

int setup_le_broadcast_msg(struct proto_instance *ins, enum le_opcode opcode) {
    int i, buf_stable_idx;
    struct consensus_priv *priv =
            (struct consensus_priv *) ins->proto_data;

    int32_t term = priv->term;
    int32_t candidate_id = priv->node_id;

    int32_t last_log_idx = priv->sm_log.stable_idx;
    int32_t last_log_term;


    if (priv->sm_log.stable_idx == -1)
        last_log_term = priv->term;
    else {
        asgard_dbg("priv->sm_log.stable_idx=%d\n", priv->sm_log.stable_idx);
        buf_stable_idx = consensus_idx_to_buffer_idx(&priv->sm_log, priv->sm_log.last_applied);

        if (buf_stable_idx == -1) {
            asgard_error("Invalid idx. could not convert to buffer idx in %s", __FUNCTION__);
            return -1;
        }
        last_log_term = priv->sm_log.entries[buf_stable_idx]->term;
    }

    for (i = 0; i < priv->sdev->pminfo.num_of_targets; i++)
        setup_le_msg(ins, &priv->sdev->pminfo, opcode, (int32_t) i, term, candidate_id, last_log_idx, last_log_term);

    return 0;
}


void accept_leader(struct proto_instance *ins, int remote_lid, int cluster_id, int32_t term) {
    struct consensus_priv *priv =
            (struct consensus_priv *) ins->proto_data;
#if VERBOSE_DEBUG
    asgard_log_le("%s, %lu, %d: accept cluster node %d with term %u as new leader\n",
        nstate_string(priv->nstate),
        ASGARD_TIMESTAMP,
        priv->term,
        cluster_id,
        term);
#endif

    priv->term = term;
    priv->leader_id = cluster_id;
    priv->sdev->cur_leader_lid = remote_lid;
    node_transition(ins, FOLLOWER);

}

void le_state_transition_to(struct consensus_priv *priv, enum le_state state) {
    asgard_dbg("Leader Election Activation State Transition from %s to %s\n", le_state_name(priv->state), le_state_name(state));
    priv->state = state;
}

int node_transition(struct proto_instance *ins, node_state_t state) {
    int err;
    struct consensus_priv *priv =
            (struct consensus_priv *) ins->proto_data;

    priv->votes = 0; // start with 0 votes on every transition

    switch (priv->nstate) {
        case FOLLOWER:
            err = stop_follower(ins);
            break;
        case CANDIDATE:
            err = stop_candidate(ins);
            break;
        case LEADER:
            err = stop_leader(ins);
            break;
        default:
            asgard_error("Unknown node state %d\n - abort", state);
            err = -EINVAL;
    }

    if(err){
        asgard_dbg("Could not stop current node role\n");
        goto error;
    }


    switch (state) {
        case FOLLOWER:
            err = start_follower(ins);
            break;
        case CANDIDATE:
            err = start_candidate(ins);

            /* If Candidature fails, stop ASGARD to prevent endless retry loop. */
            if (err) {
                asgard_pm_stop(&priv->sdev->pminfo);
            }

            break;
        case LEADER:
            err = start_leader(ins);
            write_log(&ins->logger, CANDIDATE_BECOME_LEADER, ASGARD_TIMESTAMP);
            break;
        default:
            asgard_error("Unknown node state %d\n - abort", state);
            err = -EINVAL;
    }

    if (err)
        goto error;
#if VERBOSE_DEBUG
    asgard_log_le("%s, %lu, %d: transition to state %s\n",
                nstate_string(priv->nstate),
                ASGARD_TIMESTAMP,
                priv->term,
                nstate_string(state));
#endif

    /* Persist node state in kernel space */
    priv->nstate = state;

    /* Update Node State for User Space */
    update_self_state(priv->sdev->ci, state);

    return 0;

    error:
    asgard_error(" node transition failed\n");
    return err;
}


int check_handle_nomination(struct consensus_priv *priv, uint32_t param1, uint32_t param3, uint32_t param4,
                            int rcluster_id) {
    uint32_t buf_lastidx;


    /* Learn new Term if self id is lower but incoming nomination request has higher term */
    if(priv->sdev->pminfo.cluster_id < rcluster_id)
        if(priv->term < param1) {
            priv->term = param1;
            asgard_dbg("Catching up term with ongoing election\n");
        }


    if (priv->term < param1) {
        if (priv->voted == param1) {
#if VERBOSE_DEBUG
            asgard_dbg("Voted already. Waiting for ftimeout or HB from voted leader.\n");
#endif
            return 0;
        } else {

            if (priv->sdev->cur_leader_lid >= 0 && priv->sdev->cur_leader_lid < priv->sdev->pminfo.num_of_targets)
                if (priv->sdev->pminfo.pm_targets[priv->sdev->cur_leader_lid].alive
                    && priv->leader_id < rcluster_id)
                    return 0; // current leader is alive and has better id

            // if local log is empty, just grant the vote!
            if (priv->sm_log.last_idx == -1)
                return 1;

            buf_lastidx = consensus_idx_to_buffer_idx(&priv->sm_log, priv->sm_log.last_idx);

            if (buf_lastidx == -1) {
                asgard_error("Invalid idx. could not convert to buffer idx in %s", __FUNCTION__);
                return -1;
            }
            // Safety Check during development & Debugging..
            if (priv->sm_log.entries[buf_lastidx] == NULL) {
                asgard_dbg("BUG! Log is faulty can not grant any votes. \n");
                return 0;
            }

            // candidates log is at least as up to date as the local log!
            if (param3 >= priv->sm_log.last_idx) {
                // Terms of previous log item must match with lastLogTerm of Candidate
                if (priv->sm_log.entries[buf_lastidx]->term == param4)
                    return 1;
            }

        }
    }
    return 0; // got request of invalid term! (lower or equal current term)
}


void reply_append(struct proto_instance *ins, struct pminfo *spminfo, int remote_lid, int param1, int append_success,
                  uint32_t logged_idx) {
    struct consensus_priv *priv =
            (struct consensus_priv *) ins->proto_data;

    struct asgard_payload *pkt_payload;
    unsigned char *pkt_payload_sub;

    asgard_log_le("%s, %lu, %d: REPLY APPEND state=%d, param1=%d, param3=%d, param4=%d\n",
            nstate_string(priv->nstate),
            ASGARD_TIMESTAMP,
            priv->term,
            append_success,
            param1,
            logged_idx,
            priv->sm_log.stable_idx);

    pthread_mutex_lock(&spminfo->pm_targets[remote_lid].pkt_data.mlock);

    pkt_payload =
            spminfo->pm_targets[remote_lid].pkt_data.payload;

    pkt_payload_sub =
            asgard_reserve_proto(ins->instance_id, pkt_payload, ASGARD_PROTO_CON_PAYLOAD_SZ);


    if (!pkt_payload_sub) {
        asgard_error("asgard packet full!\n");
        pthread_mutex_unlock(&spminfo->pm_targets[remote_lid].pkt_data.mlock);
        return;
    }


    set_le_opcode((unsigned char *) pkt_payload_sub, APPEND_REPLY, param1, append_success, logged_idx,
                  priv->sm_log.stable_idx);

    pthread_mutex_unlock(&spminfo->pm_targets[remote_lid].pkt_data.mlock);

    if (append_success)
        write_log(&ins->logger, REPLY_APPEND_SUCCESS, ASGARD_TIMESTAMP);
    else
        write_log(&ins->logger, REPLY_APPEND_FAIL, ASGARD_TIMESTAMP);

}


uint32_t check_prev_log_match(struct consensus_priv *priv, uint32_t prev_log_term, uint32_t prev_log_idx) {
    struct sm_log_entry *entry;
    uint32_t buf_prevlogidx;


    if (prev_log_idx == -1) {
        if (prev_log_term < priv->term) {
            asgard_error(" received append RPC with lower prev term");
            // TODO: handle this case.
        }
        return 0;
    }

    buf_prevlogidx = consensus_idx_to_buffer_idx(&priv->sm_log, prev_log_idx);

    if (buf_prevlogidx < 0) {
        asgard_dbg("Error converting consensus idx to buffer in %s", __FUNCTION__);
        return -1;
    }

    if (prev_log_idx > priv->sm_log.last_idx) {
        asgard_error("Entry at index %d does not exist. Expected stable append.\n", prev_log_idx);
        return 1;
    }

    entry = priv->sm_log.entries[buf_prevlogidx];

    if (entry == NULL) {
        asgard_dbg("Unstable commit at index %d was not detected previously! ", prev_log_idx);
        return 1;
    }

    if (entry->term != prev_log_term) {
        asgard_error("prev_log_term does not match %d != %d", entry->term, prev_log_term);

        // Delete entries from prev_log_idx to last_idx
        //remove_from_log_until_last(&priv->sm_log, prev_log_idx);
        return 1;
    }

    return 0;
}

int append_commands(struct consensus_priv *priv, unsigned char *pkt, int num_entries, int pkt_size, int start_log_idx,
                    int unstable) {
    int i, err, new_last;
    struct data_chunk *cur_ptr;

    new_last = start_log_idx + num_entries - 1;

    // check if entries would exceed pkt
    if ((num_entries * AE_ENTRY_SIZE + ASGARD_PROTO_CON_AE_BASE_SZ) > pkt_size) {
        err = -EINVAL;
        asgard_dbg("Claimed num of log entries (%d) would exceed packet size!\n", num_entries);
        goto error;
    }

    cur_ptr = (struct data_chunk *) GET_CON_PROTO_ENTRIES_START_PTR(pkt);

    for (i = start_log_idx; i <= new_last; i++) {

        /* Directly pass data ptr into pkt location */
        err = append_command(priv, (struct data_chunk *) cur_ptr, priv->term, i, unstable);

        if (err) {
            goto error;
        }
        cur_ptr++;

    }

    if (!unstable) {
        update_stable_idx(priv);
    }

    /*asgard_dbg("Handled APPEND RPC\n"
                "\t stable_idx = %d\n"
                "\t commit_idx = %d\n"
                "\t received entries [ %d - %d ]\n"
                "\t CPU = %d\n",
                priv->sm_log.stable_idx,
                priv->sm_log.commit_idx,
                start_log_idx,
                new_last,
                smp_processor_id());*/

    return 0;

    error:
    asgard_error("Failed to append commands to log\n");
    return err;
}

void _handle_append_rpc(struct proto_instance *ins, struct consensus_priv *priv, unsigned char *pkt, int remote_lid,
                        int rcluster_id) {
    uint32_t *prev_log_term;
    uint32_t num_entries;
    int32_t *prev_log_idx;

    uint16_t pkt_size;
    int unstable = 0;
    int start_idx;

    num_entries = GET_CON_AE_NUM_ENTRIES_VAL(pkt);

    priv->sdev->pminfo.pm_targets[remote_lid].received_log_replications++;

    if (num_entries == 0) {
        asgard_dbg("no entries in payload \n");
        return;    // no reply if nothing to append!
    }
    pkt_size = GET_PROTO_OFFSET_VAL(pkt);
    prev_log_term = GET_CON_AE_PREV_LOG_TERM_PTR(pkt);
    prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(pkt);

    /*   If we receive a log replication from the current leader,
     * 	 we can continue to store it even if a previous part is missing.
     *
     * We call log replications with missing parts "unstable" replications.
     * A stable index points to the last entry in the the log, where
     * all previous entries exist (stable is not necessarily commited!).
     */
    pthread_mutex_lock(&priv->sm_log.mlock);
    //mutex_lock(&priv->sm_log.mlock);
    if (*prev_log_idx < priv->sm_log.stable_idx) {
        //mutex_unlock(&priv->sm_log.mlock);
        asgard_error("prev log idx is smaller than stable index!\n");
        pthread_mutex_unlock(&priv->sm_log.mlock);
        return;
    }

    if (*prev_log_idx > priv->sm_log.stable_idx) {
        /* Only accept unstable entries if leader and term did not change!
         *
         *   If we have a leader change, we must reset last index to the stable index,
         *   and continue to build the log from there (throw away unstable items).
         */
        if (*prev_log_term == priv->term && priv->leader_id == rcluster_id) {

            unstable = 1;
            asgard_dbg("Unstable log rep detected!\n");

        } else {
            asgard_dbg("Case unhandled!\n");
        }
    }

    start_idx = (*prev_log_idx) + 1;

    if (check_prev_log_match(priv, *prev_log_term, priv->sm_log.stable_idx)) {
        asgard_dbg("Log inconsitency detected. prev_log_term=%d, prev_log_idx=%d\n",
                   *prev_log_term, *prev_log_idx);

        print_log_state(&priv->sm_log);

        goto reply_false_unlock;
    }

    if (append_commands(priv, pkt, num_entries, pkt_size, start_idx, unstable)) {
        asgard_dbg("append commands failed. start_idx=%d, unstable=%d\n", start_idx, unstable);
        goto reply_false_unlock;
    }

    update_next_retransmission_request_idx(priv);

    if (unstable) {
        asgard_dbg("[Unstable] appending entries %d - %d | re_idx=%d | stable_idx=%d\n",
         	start_idx, start_idx + num_entries, priv->sm_log.next_retrans_req_idx, priv->sm_log.stable_idx);
        priv->sm_log.unstable_commits++;
    } else {
        asgard_dbg("[Stable] appending entries %d - %d\n", start_idx, start_idx + num_entries);
    }

    if (priv->sm_log.next_retrans_req_idx != -2)
        goto reply_retransmission;

// default: reply success
    pthread_mutex_unlock(&priv->sm_log.mlock);

    if (priv->sdev->multicast.enable)
        return;

    reply_append(ins, &priv->sdev->pminfo, remote_lid, priv->term, 1, priv->sm_log.stable_idx);

    priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;
    return;

reply_retransmission:
    pthread_mutex_unlock(&priv->sm_log.mlock);

    // TODO: wait until other pending workers are done, and check again if we need a retransmission!
    reply_append(ins, &priv->sdev->pminfo, remote_lid, priv->term, 2, priv->sm_log.next_retrans_req_idx);
    priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;
    return;
reply_false_unlock:
    pthread_mutex_unlock(&priv->sm_log.mlock);
    reply_append(ins, &priv->sdev->pminfo, remote_lid, priv->term, 0, priv->sm_log.stable_idx);
    priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;
}


void reply_vote(struct proto_instance *ins, int remote_lid, int rcluster_id, int32_t param1, int32_t param2) {
    struct consensus_priv *priv =
            (struct consensus_priv *) ins->proto_data;

#if VERBOSE_DEBUG
    asgard_log_le("%s, %lu, %d: voting for cluster node %d with term %d\n",
        nstate_string(priv->nstate),
        ASGARD_TIMESTAMP,
        priv->term,
        rcluster_id,
        param1);
#endif

    setup_le_msg(ins, &priv->sdev->pminfo, VOTE, remote_lid, param1, param2, 0, 0);
    priv->voted = param1;

    priv->sdev->pminfo.pm_targets[remote_lid].fire = 1;

    write_log(&ins->logger, VOTE_FOR_CANDIDATE, ASGARD_TIMESTAMP);

}

void check_pending_log_rep_for_multicast(struct asgard_device *sdev)
{
    int32_t next_index;
    int retrans = 0;

    if(sdev->is_leader == 0)
        return;

    if(sdev->pminfo.state != ASGARD_PM_EMITTING)
        return;

    next_index = sdev->multicast.nextIdx;

    if(next_index < 0)
        return;
    asgard_dbg("multicast schedule called\n");
    schedule_log_rep(sdev, 0, next_index, retrans, 1);
}



void set_all_targets_dead(struct asgard_device *sdev)
{
    struct pminfo *spminfo = &sdev->pminfo;
    int i;

    for (i = 0; i < spminfo->num_of_targets; i++)
        spminfo->pm_targets[i].alive = 0;

}


int consensus_is_alive(struct consensus_priv *priv)
{
    if(!priv)
        return 0;

    if (priv->state != LE_RUNNING)
        return 0;

    return 1;
}

int consensus_post_ts(struct proto_instance *ins, unsigned char *remote_mac,
                      uint64_t ts) {
    struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;

    if (!consensus_is_alive(priv))
        return 0;

    return 0;
}




int consensus_start(struct proto_instance *ins) {
    int err;

    struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;

    if (consensus_is_alive(priv)) {
        asgard_dbg("Consensus is already running!\n");
        return 0;
    }

    le_state_transition_to(priv, LE_RUNNING);

    logger_state_transition_to(&ins->logger, LOGGER_RUNNING);

    err = node_transition(ins, FOLLOWER);

    if (err)
        goto error;

    asgard_dbg("===================== Start of Run ====================\n" );

    return 0;

    error:
    asgard_error(" %s failed\n", __func__);
    return err;
}


int consensus_stop(struct proto_instance *ins) {
    struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;
    int i;

    if (!consensus_is_alive(priv))
        return 0;

    asgard_dbg("===================== End of Run: ====================\n" );

    asgard_dbg("Transmitted Packets:%lu\n", priv->sdev->tx_counter);

    // Dump Logs to File

    for(i = 1; i <= priv->sdev->pminfo.num_of_targets + 1; i++){
        dump_ingress_log(&ins->ingress_logger.per_node_logger[i],  i, priv->sdev->pminfo.hbi);
    }

    switch (priv->nstate) {
        case FOLLOWER:
            stop_follower(ins);
            break;
        case CANDIDATE:
            stop_candidate(ins);
            break;
        case LEADER:
            stop_leader(ins);
            break;
    }

    le_state_transition_to(priv, LE_READY);

    set_all_targets_dead(priv->sdev);

    return 0;
}

int consensus_clean(struct proto_instance *ins) {
    struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;
    uint32_t i;

    le_state_transition_to(priv, LE_READY);

    if (consensus_is_alive(priv)) {
        asgard_dbg("Consensus is running, stop it first.\n");
        return 0;
    }

    le_state_transition_to(priv, LE_UNINIT);

    //remove_eval_ctrl_interfaces(priv);

    //remove_le_config_ctrl_interfaces(priv);

    clear_logger(&ins->logger);
    clear_ingress_logger(&ins->ingress_logger);

    clear_logger(&priv->throughput_logger);
    clear_logger(&ins->user_a);
    clear_logger(&ins->user_b);
    clear_logger(&ins->user_c);
    clear_logger(&ins->user_d);

    for (i = 0; i < priv->sm_log.max_entries; i++) {
        if (priv->sm_log.entries[i] != NULL) {
            if (priv->sm_log.entries[i]->dataChunk != NULL) {
                free(priv->sm_log.entries[i]->dataChunk);
            }
            free(priv->sm_log.entries[i]);
        }
    }

    /* Clean Follower (RX) Synbuf */
    // synbuf_chardev_exit(priv->synbuf_rx);
    free(priv->txbuf);

    /* Clean Leader (TX) Synbuf  */
    // synbuf_chardev_exit(priv->synbuf_tx);
    free(priv->rxbuf);

    return 0;
}

int consensus_info(struct proto_instance *ins) {
    asgard_dbg("consensus info\n");
    return 0;
}

int consensus_us_update(struct proto_instance *ins, void *payload) {
    asgard_dbg("consensus update\n");

    return 0;
}

int consensus_post_payload(struct proto_instance *ins, int remote_lid,
                           int rcluster_id, void *payload) {
    struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;

    if (!consensus_is_alive(priv))
        return 0;

    // safety check during debugging and development
    if (!priv) {
        asgard_dbg("private consensus data is null!\n");
        return 0;
    }

    switch (priv->nstate) {
        case FOLLOWER:
            follower_process_pkt(ins, remote_lid, rcluster_id, payload);
            break;
        case CANDIDATE:
            candidate_process_pkt(ins, remote_lid, rcluster_id, payload);
            break;
        case LEADER:
            leader_process_pkt(ins, remote_lid, rcluster_id, payload);
            break;
        default:
            asgard_error("Unknown state - BUG\n");
            break;
    }

    return 0;
}


int consensus_init(struct proto_instance *ins) {
    struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;
    int i;
    size_t psize = getpagesize();

    priv->voted = -1;
    priv->term = 0;
    priv->state = LE_READY;

    priv->sm_log.last_idx = -1;
    priv->sm_log.commit_idx = -1;
    priv->sm_log.stable_idx = -1;
    priv->sm_log.next_retrans_req_idx = -2;
    priv->sm_log.last_applied = -1;
    priv->sm_log.max_entries = MAX_CONSENSUS_LOG;
    priv->sm_log.lock = 0;
    priv->sdev->consensus_priv = priv; /* reference for pacemaker */
    priv->sdev->cur_leader_lid = -1;
    priv->sm_log.entries = malloc(MAX_CONSENSUS_LOG * sizeof(struct sm_log_entry *));

    if (!priv->sm_log.entries){
        asgard_dbg("Failed to allocate memory for log entries\n");
        return -1;
    }

    /* Pre Allocate Buffer Entries */
    for (i = 0; i < MAX_CONSENSUS_LOG; i++) {
        // Both freed on consensus clean
        priv->sm_log.entries[i] = malloc(sizeof(struct sm_log_entry));
        priv->sm_log.entries[i]->dataChunk = malloc(sizeof(struct data_chunk));
        priv->sm_log.entries[i]->valid = 0;
    }


    // requires "proto_instances/%d"
    //init_le_config_ctrl_interfaces(priv);

    // requires "proto_instances/%d"
    //init_eval_ctrl_interfaces(priv);

    // requires "proto_instances/%d"
    init_logger(&ins->logger, ins->instance_id, priv->sdev->ifindex,"consensus_le", 0);

    init_ingress_logger(&ins->ingress_logger, ins->instance_id);

    init_logger(&ins->user_a, ins->instance_id, priv->sdev->ifindex, "user_a", 1);
    init_logger(&ins->user_b, ins->instance_id, priv->sdev->ifindex, "user_b", 1);
    init_logger(&ins->user_c, ins->instance_id, priv->sdev->ifindex, "user_c", 1);
    init_logger(&ins->user_d, ins->instance_id, priv->sdev->ifindex, "user_d", 1);

    init_logger(&priv->throughput_logger, ins->instance_id, priv->sdev->ifindex,"consensus_throughput", 0);
    priv->throughput_logger.state = LOGGER_RUNNING;
    priv->throughput_logger.first_ts = 0;
    priv->throughput_logger.applied = 0;
    priv->throughput_logger.last_ts = 0;

    // No need for synbuf in user space only version -> use memory
    priv->rxbuf = malloc(250 * 20 * psize);
    priv->txbuf = malloc(250 * 20 * psize);

    /* Initialize RingBuffer */
    setup_asg_ring_buf((struct asg_ring_buf *)priv->txbuf, 1000000);

    /* Initialize RingBuffer */
    setup_asg_ring_buf((struct asg_ring_buf *)priv->rxbuf, 1000000);

    return 0;
}


int consensus_init_payload(void *payload) { return 0; }


static const struct asgard_protocol_ctrl_ops consensus_ops = {
        .init = consensus_init,
        .start = consensus_start,
        .stop = consensus_stop,
        .clean = consensus_clean,
        .info = consensus_info,
        .post_payload = consensus_post_payload,
        .post_ts = consensus_post_ts,
        .init_payload = consensus_init_payload,
        .us_update = consensus_us_update,
};




struct proto_instance *get_consensus_proto_instance(struct asgard_device *sdev)
{
    struct consensus_priv *cpriv;
    struct proto_instance *ins;

    // freed by get_echo_proto_instance
    ins = malloc (sizeof(struct proto_instance));

    if (!ins)
        goto error;

    ins->proto_type = ASGARD_PROTO_CONSENSUS;
    ins->ctrl_ops = consensus_ops;

    ins->logger.name = "consensus_le";
    ins->logger.instance_id = ins->instance_id;
    ins->logger.ifindex = sdev->ifindex;

    ins->proto_data = malloc(sizeof(struct consensus_priv));

    cpriv = (struct consensus_priv *)ins->proto_data;

    if (!cpriv)
        goto error;

    cpriv->throughput_logger.instance_id = ins->instance_id;
    cpriv->throughput_logger.ifindex = sdev->ifindex;
    cpriv->throughput_logger.name = "consensus_throughput";

    cpriv->state = LE_UNINIT;

    cpriv->max_entries_per_pkt = MAX_AE_ENTRIES_PER_PKT;
    cpriv->sdev = sdev;
    cpriv->ins = ins;
    cpriv->llts_before_ftime = 0;

    return ins;

    error:
    asgard_dbg("Error in %s", __func__);
    return NULL;
}

