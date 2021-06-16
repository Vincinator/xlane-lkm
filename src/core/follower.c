

#include "follower.h"


#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][LE][FOLLOWER]"


int follower_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
{
    struct consensus_priv *priv =
            (struct consensus_priv *)ins->proto_data;

    uint8_t opcode = GET_CON_PROTO_OPCODE_VAL(pkt);
    int32_t param1, param2, param3, param4, *commit_idx;

    param1 = GET_CON_PROTO_PARAM1_VAL(pkt);

    log_le_rx(priv->nstate, ASGARD_TIMESTAMP, priv->term, opcode, rcluster_id, param1);

    switch (opcode) {
        // param1 interpreted as term
        // param2 interpreted as candidateID
        // param3 interpreted as lastLogIndex of Candidate
        // param4 interpreted as lastLogTerm of Candidate
        case ADVERTISE:
            break;
        case VOTE:
            break;
        case NOMI:
            param2 = GET_CON_PROTO_PARAM2_VAL(pkt);
            param3 = GET_CON_PROTO_PARAM3_VAL(pkt);
            param4 = GET_CON_PROTO_PARAM4_VAL(pkt);
            if (check_handle_nomination(priv, param1, param3, param4, rcluster_id)) {
                reply_vote(ins, remote_lid, rcluster_id, param1, param2);
            }
            break;
        case NOOP:

            break;
        case ALIVE:

            param2 = GET_CON_PROTO_PARAM2_VAL(pkt);

            /* Received an ALIVE operation from a node that claims to be the new leader
             */
            if (param1 > priv->term) {

                accept_leader(ins, remote_lid, rcluster_id, param1);
                write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, ASGARD_TIMESTAMP);

#if VERBOSE_DEBUG
				asgard_dbg("Received ALIVE op from new leader with higher term=%d local term=%d\n", param1, priv->term);
#endif
            }

            // Current Leader is commiting
            if(param1 == priv->term && param2 != -1)
                commit_log(priv, param2);

            /* Ignore other cases for ALIVE operation*/
            break;
        case APPEND:
            if(priv->verbosity != 0)
                asgard_dbg("APPEND from %d with prev_log_idx=%d leader_commit_idx=%d\n",
                       rcluster_id,
                       *GET_CON_AE_PREV_LOG_IDX_PTR(pkt),
                       *GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt));

            if(priv->leader_id != rcluster_id) {

                asgard_error("received APPEND from a node (%d) that is not accepted as leader (%d) \n",
                             rcluster_id, priv->leader_id);
                break;
            }
            /* Received a LEAD operation from a node with a higher term,
             * thus this node was accecpted
             */
            if (param1 > priv->term) {
                accept_leader(ins, remote_lid, rcluster_id, param1);
                // write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, ASGARD_TIMESTAMP);

                _handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);

#if VERBOSE_DEBUG
				asgard_dbg("Received APPEND op from leader with higher term=%d local term=%d\n", param1, priv->term);
#endif
            }

                /* Received a LEAD operation from a node with the same term,
                 * thus, this node has to check whether it is already following
                 * that node (continue to follow), or if it is a LEAD operation
                 * from a node that is currently not the follower (Ignore and let
                 * the timeout continue to go down).
                 */
            else if (param1 == priv->term) {

                if (priv->leader_id == rcluster_id) {

                    _handle_append_rpc(ins, priv, pkt, remote_lid, rcluster_id);

                    commit_idx = GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt);
                    commit_log(priv, *commit_idx);

                } else {
#if VERBOSE_DEBUG
                    asgard_dbg("Received message from new leader (%d)! Leader changed but term did not update! term=%u\n",
					rcluster_id, param1);
#endif
                    // Ignore this LEAD message, let the ftimer continue.. because: "this is not my leader!"
                }
            }
                /* Received a LEAD operation from a node with a lower term.
                 * Ignoring this LEAD operation and let the countdown continue to go down.
                 */
            else {

#if VERBOSE_DEBUG
                //if (sdev->verbose >= 5)
				asgard_dbg("Received APPEND from leader (%d) with lower term=%u\n", rcluster_id, param1);
#endif
                if (priv->leader_id == rcluster_id) {
                    asgard_error("BUG! Current accepted LEADER (%d) has lower Term=%u\n", rcluster_id,  param1);
                }

                // Ignore this LEAD message, let the ftimer continue.
            }

            break;
        default:
            asgard_dbg("Unknown opcode received from host: %d - opcode: %d\n", rcluster_id, opcode);

    }

    return 0;
}

void validate_log(struct state_machine_cmd_log *log)
{
    int i, err = 0;

    asgard_dbg("Log Validation Results: \n");

    for(i = 0; i < log->max_entries; i++) {

        if(log->entries[i] == NULL) {

            if(log->stable_idx > i) {
                asgard_error("\t Stable Index is incorrect.\n");
                asgard_error("\t\t stable_idx=%d, but log entry %d is NULL\n", log->stable_idx, i);
                err++;
            }

            if(log->commit_idx > i) {
                asgard_error("\t Commit Index is incorrect. \n");
                asgard_error("\t\t commit_idx=%d, but log entry %d is NULL\n", log->commit_idx, i);
                err++;
            }

        } else {
            if(log->last_idx < i) {
                asgard_error("\t No entries allowed after last index\n");
                asgard_error("\t\t last index =%d, but log entry %d exists\n", log->last_idx, i);
                err++;
            }
        }
    }

    if(log->stable_idx > log->commit_idx) {
        asgard_error("\t Commit index did not keep up with Stable Index \n");
        asgard_error("\t\t commit_idx=%d, stable_idx=%d\n", log->commit_idx, log->stable_idx);
    }

    if(log->stable_idx < log->commit_idx) {
        asgard_error("\t Commit index points to an unstable log entry \n");
        asgard_error("\t\t commit_idx=%d, stable_idx=%d\n", log->commit_idx, log->stable_idx);
    }

    asgard_dbg("\t Detected %d Errors in Log \n", err);

}

void print_follower_stats(struct consensus_priv *priv)
{
    int i;

    for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++){
        asgard_dbg("Target infos %d:", i);
        asgard_dbg("\t pkt TX counter: %d\n",  priv->sdev->pminfo.pm_targets[i].pkt_tx_counter);
        asgard_dbg("\t pkt RX counter: %d\n", priv->sdev->pminfo.pm_targets[i].pkt_rx_counter);
        asgard_dbg("\t received log reps(all): %d\n", priv->sdev->pminfo.pm_targets[i].received_log_replications);
    }

    asgard_dbg("unstable commits %d\n", priv->sm_log.unstable_commits );
    asgard_dbg("last_idx %d \n", priv->sm_log.last_idx );
    asgard_dbg("stable_idx %d\n", priv->sm_log.stable_idx );
    asgard_dbg("next_retrans_req_idx %d\n", priv->sm_log.next_retrans_req_idx );
    asgard_dbg("max_entries %d\n", priv->sm_log.max_entries );

    /* TODO: redo for ring buffer implementation */
    //validate_log(&priv->sm_log);

}


int stop_follower(struct proto_instance *ins)
{
    struct consensus_priv *priv =
            (struct consensus_priv *)ins->proto_data;

    print_follower_stats(priv);

    return 0;
}

int start_follower(struct proto_instance *ins)
{
    int err;
    struct consensus_priv *priv =
            (struct consensus_priv *)ins->proto_data;

    err = setup_le_broadcast_msg(ins, NOOP);

    if (err)
        goto error;

    priv->sdev->tx_port = 3319;
    priv->sdev->is_leader = 0;
    priv->sm_log.unstable_commits = 0;
    priv->sm_log.turn = 0;

    asg_mutex_init(&priv->sm_log.mlock);
    asg_mutex_init(&priv->sm_log.next_lock);
    asg_mutex_init(&priv->accept_vote_lock);

    priv->votes = 0;
    priv->nstate = FOLLOWER;
    priv->candidate_counter = 0;

#if VERBOSE_DEBUG
    asgard_dbg("Node became a follower\n");
#endif

    return 0;

    error:
    asgard_error("Failed to start as follower\n");
    return err;
}


