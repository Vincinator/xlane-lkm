



#include "kvstore.h"


int32_t get_last_idx_safe(struct consensus_priv *priv)
{
    if (priv->sm_log.last_idx < -1)
        priv->sm_log.last_idx = -1; // repair last index!

    return priv->sm_log.last_idx;
}


int32_t get_prev_log_term(struct consensus_priv *cur_priv, int32_t con_idx)
{

    int32_t idx;


    if (con_idx == -1) {
        // Logs are empty, because next index points to first element.
        // Thus, there was no prev log term. And therefore we can use the
        // current term.
        return cur_priv->term;
    }

    if (con_idx < -1) {
        asgard_dbg("invalid value - idx=%d\n", idx);
        return -1;
    }

    idx = consensus_idx_to_buffer_idx(&cur_priv->sm_log, con_idx);

    if(idx < 0) {
        asgard_dbg("Error converting consensus idx to buffer in %s", __FUNCTION__);
        return -1;
    }

    if (idx > cur_priv->sm_log.last_idx) {
        asgard_dbg("idx %d > cur_priv->sm_log.last_id (%d) \n", idx, cur_priv->sm_log.last_idx);

        return -1;
    }

    if (idx > MAX_CONSENSUS_LOG) {
        asgard_dbg("BUG! idx (%d) > MAX_CONSENSUS_LOG (%d) \n", idx, MAX_CONSENSUS_LOG);
        return -1;
    }

    if (!cur_priv->sm_log.entries[idx]) {
        asgard_dbg("BUG! entries is null at index %d\n", idx);
        return -1;
    }

    return cur_priv->sm_log.entries[idx]->term;
}

int consensus_idx_to_buffer_idx(struct state_machine_cmd_log *log, uint32_t dividend)
{
    uint32_t divisor = log->max_entries;
    uint32_t remainder = dividend % divisor;

    //div_uint64_t_rem(dividend, divisor, &remainder);

    if(remainder < 0 || remainder > log->max_entries){
        asgard_error("init_error converting consensus idx to buf_log idx\n");
        return -1;
    }

    return remainder;
}

int append_command(struct consensus_priv *priv, struct data_chunk *dataChunk, int32_t term, int log_idx, int unstable)
{
    int err;
    struct sm_log_entry *entry;
    int buf_logidx;

    if (!priv) {
        err = -EINVAL;
        asgard_error("Priv ptr points to NULL\n");
        goto error;
    }

    if (priv->sm_log.commit_idx > log_idx) {
        err = -EPROTO;
        asgard_error("BUG - commit_idx=%d is greater than idx(%d) of entry to commit!\n", priv->sm_log.commit_idx, log_idx);
        goto error;
    }

    buf_logidx = consensus_idx_to_buffer_idx(&priv->sm_log, log_idx);

    if( buf_logidx < 0 ){
        err = -EINVAL;
        goto error;
    }

    entry = priv->sm_log.entries[buf_logidx];

    /*if(entry->valid == 1) {
        asgard_error("WARNING: Overwriting data! \n");
    }*/

    // asgard_dbg("appending to buf_idx: %d\n", buf_logidx);
    //print_hex_dump(KERN_DEBUG, "append data:", DUMP_PREFIX_NONE, 16,1,
    //              dataChunk, sizeof(struct data_chunk), 0);

    /* Write request to ASGARD Kernel Buffer */
    memcpy(entry->dataChunk->data, dataChunk, sizeof(struct data_chunk));
    entry->term = term;
    entry->valid = 1;

    if (priv->sm_log.last_idx < log_idx)
        priv->sm_log.last_idx = log_idx;

    if(log_idx == 0){
        asgard_dbg("Appended first Entry to log\n");
        write_log(&priv->ins->logger, START_LOG_REP, ASGARD_TIMESTAMP);
    }

    return 0;
    error:
    asgard_dbg("Could not append command to Logs!\n");
    return err;
}



void update_stable_idx(struct consensus_priv *priv)
{
    int32_t i;
    int cur_buf_idx;

    /* Fix stable index after stable append
     *
     * We must use the consensus indices for this loop
     * because stable_idx will be set to the first entry not null.

     */
    for (i = priv->sm_log.stable_idx + 1; i <= priv->sm_log.last_idx; i++) {

        cur_buf_idx = consensus_idx_to_buffer_idx(&priv->sm_log, i);

        if(cur_buf_idx == -1) {
            asgard_error("Invalid idx. could not convert to buffer idx in %s",__FUNCTION__);
            return;
        }

        if (priv->sm_log.entries[cur_buf_idx]->valid == 0)
            break; // stop at first invalidated entry

        if(i == 83890432) {
            asgard_error("Black magic fuckery happening right HERE.\n");
        }

        priv->sm_log.stable_idx = i; // i is a real consensus index (non modulo)
    }

}

void print_log_state(struct state_machine_cmd_log *log)
{

    asgard_dbg("\tlast_applied=%d\n", log->last_applied);

    asgard_dbg("\tlast_idx=%d\n", log->last_idx);

    asgard_dbg("\tstable_idx=%d\n", log->stable_idx);

    asgard_dbg("\tmax_entries=%d\n", log->max_entries);

    asgard_dbg("\tlock=%d\n", log->lock);

}


int apply_log_to_sm(struct consensus_priv *priv)
{
    struct state_machine_cmd_log *log;
    int applying;
    int i, buf_idx;

    log = &priv->sm_log;

    applying = log->commit_idx - (log->last_applied == -1 ? 0 : log->last_applied);

    write_log(&priv->throughput_logger, applying, ASGARD_TIMESTAMP);

    if(priv->throughput_logger.first_ts == 0){
        priv->throughput_logger.first_ts = ASGARD_TIMESTAMP;
    }

    priv->throughput_logger.last_ts = ASGARD_TIMESTAMP;
    priv->throughput_logger.applied += applying;


    if(!priv->rxbuf ) {
        asgard_error("Buffer is not initialized!\n");
        return -1;
    }

    for(i = log->last_applied + 1; i <= log->commit_idx; i++) {

        buf_idx =  consensus_idx_to_buffer_idx(&priv->sm_log, i);

        if(buf_idx == -1) {
            return -1;
        }

        asgard_dbg("applying consensus entry: %d, and buf_idx: %d\ n", i, buf_idx);
        asgard_dbg("log->next_index %d \n", log->next_index);

        // TODO: is the datachunk ready to append!?

        if(append_rb(priv->rxbuf, log->entries[buf_idx]->dataChunk)) {
            // asgard_error("Could not append to ring buffer tried to append index %i buf_idx:%d!\n", i,  buf_idx);
            return -1;
        }

        log->entries[buf_idx]->valid = 0; // can overwrite this ASGARD LOG element again
        log->last_applied++;
    }



    //asgard_dbg("Added %d commands to State Machine.\n", applying);

    return 0;
}

int commit_log(struct consensus_priv *priv, int32_t commit_idx)
{
    int err = 0;
    struct state_machine_cmd_log *log = &priv->sm_log;

    asg_mutex_lock(&priv->sm_log.mlock);

    // Check if commit index must be updated
    if (commit_idx > priv->sm_log.commit_idx) {

        if(!priv->sdev->multicast.enable && commit_idx > priv->sm_log.stable_idx){
            asgard_error("Commit idx is greater than local stable idx\n");
            asgard_dbg("\t leader commit idx: %d, local stable idx: %d\n", commit_idx, priv->sm_log.stable_idx);
        } else {
            priv->sm_log.commit_idx = commit_idx;
            err = apply_log_to_sm(priv);

            if (!err)
                write_log(&priv->ins->logger, GOT_CONSENSUS_ON_VALUE, ASGARD_TIMESTAMP);
        }
    }

    asg_mutex_unlock(&priv->sm_log.mlock);

    if(err)
        asgard_dbg("Could not apply logs. Commit Index %d\n", log->commit_idx);

    return 0;

}