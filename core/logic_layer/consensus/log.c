#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>

#include <linux/slab.h>
#include <linux/kernel.h>

#include "include/leader.h"
#include <asguard/consensus.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][RSM]"


int apply_log_to_sm(struct consensus_priv *priv)
{
	struct state_machine_cmd_log *log;
	int applying;
    int i, buf_idx;

	log = &priv->sm_log;

	applying = log->commit_idx - (log->last_applied == -1 ? 0 : log->last_applied);

	write_log(&priv->throughput_logger, applying, RDTSC_ASGUARD);

	if(!priv->synbuf_rx || !priv->synbuf_rx->ubuf) {
	    asguard_error("synbuf is not initialized!\n");
	    return -1;
	}

    for(i = log->last_applied + 1; i <= log->commit_idx; i++) {

        buf_idx =  consensus_idx_to_buffer_idx(&priv->sm_log, i);

        if(buf_idx == -1) {
            return -1;
        }

        if(append_rb((struct asg_ring_buf *) priv->synbuf_rx->ubuf, log->entries[buf_idx]->dataChunk)) {
            asguard_error("Could not append to ring buffer tried to append index %i buf_idx:%d!\n", i,  buf_idx);
            return -1;
        }

        log->entries[buf_idx]->valid = 0; // element can be overwritten now
        log->last_applied++;
    }


	//asguard_dbg("Added %d commands to State Machine.\n", applying);

	return 0;
}

int commit_log(struct consensus_priv *priv, s32 commit_idx)
{
	int err = 0;
	struct state_machine_cmd_log *log = &priv->sm_log;

    mutex_lock(&priv->sm_log.mlock);

    // Check if commit index must be updated
    if (commit_idx > priv->sm_log.commit_idx) {
        if(commit_idx > priv->sm_log.stable_idx){
            asguard_error("Commit idx is greater than local stable idx\n");
            asguard_dbg("\t leader commit idx: %d, local stable idx: %d\n", commit_idx, priv->sm_log.stable_idx);
        } else {
            priv->sm_log.commit_idx = commit_idx;
            err = apply_log_to_sm(priv);

            if (!err)
                write_log(&priv->ins->logger, GOT_CONSENSUS_ON_VALUE, RDTSC_ASGUARD);
        }
    }

    mutex_unlock(&priv->sm_log.mlock);

    if(err)
        asguard_dbg("Could not apply logs. Commit Index %d\n", log->commit_idx);

    return 0;

}
EXPORT_SYMBOL(commit_log);


void print_log_state(struct state_machine_cmd_log *log)
{

	asguard_dbg("\tlast_applied=%d\n", log->last_applied);

	asguard_dbg("\tlast_idx=%d\n", log->last_idx);

	asguard_dbg("\tstable_idx=%d\n", log->stable_idx);

	asguard_dbg("\tmax_entries=%d\n", log->max_entries);

	asguard_dbg("\tlock=%d\n", log->lock);

}
EXPORT_SYMBOL(print_log_state);


void update_stable_idx(struct consensus_priv *priv)
{
	int i;
	int cur_buf_idx;

    /* Stable idx is already good */
    if(priv->sm_log.stable_idx == priv->sm_log.last_idx)
        return;

	/* Fix stable index after stable append
	 *
	 * We must use the consensus indices for this loop
	 * because stable_idx will be set to the first entry not null.
	 *
	 *
	 */
	for (i = priv->sm_log.stable_idx; i <= priv->sm_log.last_idx; i++) {

        cur_buf_idx = consensus_idx_to_buffer_idx(&priv->sm_log, i);

        if(cur_buf_idx == -1) {
            asguard_error("Invalid idx. could not convert to buffer idx in %s",__FUNCTION__);
            return;
        }

		if (!priv->sm_log.entries[cur_buf_idx])
			break; // stop at first missing entry

		priv->sm_log.stable_idx = i; // i is a real consensus index (non modulo)
	}

}
EXPORT_SYMBOL(update_stable_idx);


void update_next_retransmission_request_idx(struct consensus_priv *priv)
{
	int i;
	int first_re_idx = -2;
	int cur_idx = -2;
	int skipped = 0;
	int cur_buf_idx;

	if(priv->sm_log.last_idx == -1){
		asguard_dbg("Nothing has been received yet!\n");
		return;
	}

	/* stable_idx + 1 always points to an invalid entry and
	 * if stable_idx != last_idx is also true, we have found
	 * a missing entry. The latter case is true for all loop iterations
	 *
	 *
	 */
	for(i = priv->sm_log.stable_idx + 1; i < priv->sm_log.last_idx; i++) {

		// if request has already been sent, skip indicies that may be included
		// ... in the next log replication packet
		if(priv->sm_log.next_retrans_req_idx == i){
			i += priv->max_entries_per_pkt - 1;
			skipped = 1;
			continue;
		}
        cur_buf_idx = consensus_idx_to_buffer_idx(&priv->sm_log, i);

        if(cur_buf_idx == -1) {
            asguard_error("Invalid idx. could not convert to buffer idx in %s",__FUNCTION__);
            return;
        }

		if(!priv->sm_log.entries[cur_buf_idx]){
			cur_idx = i;

			if (first_re_idx == -2)
				first_re_idx = i;

			// we can use first found idx
			if(priv->sm_log.next_retrans_req_idx == -2)
				break;

			// found next missing entry after prev request
			if(i > priv->sm_log.next_retrans_req_idx)
				break;
		}
	}

	// note: with next_retrans_req_idx = -2, we would start with requesting the last missing idx
	// .... this should not be a problem though
	priv->sm_log.next_retrans_req_idx
			= priv->sm_log.next_retrans_req_idx < cur_idx ? cur_idx : first_re_idx;
}
EXPORT_SYMBOL(update_next_retransmission_request_idx);


int consensus_idx_to_buffer_idx(struct state_machine_cmd_log *log, u32 dividend)
{
    u32 divisor = log->max_entries;
    u32 remainder = 0;

    div_u64_rem(dividend, divisor, &remainder);


    if(remainder < 0 || remainder > log->max_entries){
        asguard_error("error converting consensus idx to buf_log idx\n");
        return -1;
    }

    return remainder;
}
EXPORT_SYMBOL(consensus_idx_to_buffer_idx);


int append_command(struct consensus_priv *priv, struct data_chunk *dataChunk, s32 term, int log_idx, int unstable)
{
	int err;
	struct sm_log_entry *entry;
    int buf_logidx, buf_appliedidx,buf_commitidx;

	if (!priv) {
		err = -EINVAL;
		asguard_error("Priv ptr points to NULL\n");
		goto error;
	}

	if (priv->sm_log.commit_idx > log_idx) {
		err = -EPROTO;
		asguard_error("BUG - commit_idx=%d is greater than idx(%d) of entry to commit!\n", priv->sm_log.commit_idx, log_idx);
		goto error;
	}

    buf_logidx = consensus_idx_to_buffer_idx(&priv->sm_log, log_idx);
    buf_appliedidx = consensus_idx_to_buffer_idx(&priv->sm_log, priv->sm_log.last_applied);
    buf_commitidx = consensus_idx_to_buffer_idx(&priv->sm_log, priv->sm_log.commit_idx);

    if( buf_logidx < 0 ){
        err = -EINVAL;
        goto error;
    }

    if( (log_idx != 0) && (buf_appliedidx == -1 || buf_commitidx == -1) ){
        err = -EINVAL;
        goto error;
    }

    /* Never write between applied and commit_idx!
     *
     * Note: on leader we set the applied_idx equal commit_id,
     *       because the leader received the request from the
     *       user space, thus nothing has to be applied back
     *       to user space. Because we are setting
     *       applied_idx = commit_idx, the leader won't run into any
     *       troubles in this guard.
     *
     * If log_idx is equal to 0, then applied and commit idx are initialized to -1
     * but we are safe to write!
     */
    if( (log_idx != 0) && !(buf_logidx < buf_appliedidx || buf_logidx > buf_commitidx) ) {
        asguard_error("Invalid Idx to write! \n");
        err = -EINVAL;
        goto error;
    }

    entry = priv->sm_log.entries[buf_logidx];

    if(entry->valid == 1) {
        asguard_error("WARNING: Overwriting data! \n");
    }

    /* Write request to ASGARD Kernel Buffer */
    memcpy(entry->dataChunk, dataChunk, sizeof(struct data_chunk));
	entry->term = term;
	entry->valid = 1;

	if (priv->sm_log.last_idx < log_idx)
		priv->sm_log.last_idx = log_idx;

	if(log_idx == 0){
		asguard_dbg("Appended first Entry to log\n");
		write_log(&priv->ins->logger, START_LOG_REP, RDTSC_ASGUARD);
	}

	return 0;
error:
	asguard_dbg("Could not append command to Logs!\n");
	return err;
}
EXPORT_SYMBOL(append_command);


