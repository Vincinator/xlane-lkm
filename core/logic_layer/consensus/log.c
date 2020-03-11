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

	log = &priv->sm_log;
	applying = log->commit_idx - log->last_applied;

	write_log(&priv->throughput_logger, applying, RDTSC_ASGUARD);

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

            if (!err) {
                log->last_applied = log->commit_idx;
                write_log(&priv->ins->logger, GOT_CONSENSUS_ON_VALUE, RDTSC_ASGUARD);
            }
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
		// fix stable index after stable append
	for (i = 0; i <= priv->sm_log.last_idx; i++) {

		if (!priv->sm_log.entries[i]) // stop at first missing entry
			break;

		priv->sm_log.stable_idx = i;
	}

}
EXPORT_SYMBOL(update_stable_idx);


void update_next_retransmission_request_idx(struct consensus_priv *priv)
{
	int i;
	int first_re_idx = -2;
	int cur_idx = -2;
	int skipped = 0;

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

		if(!priv->sm_log.entries[i]){
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


int append_command(struct consensus_priv *priv, struct sm_command *cmd, s32 term, int log_idx, int unstable)
{
	int err;
	struct sm_log_entry *entry;

	if (!priv) {
		err = -EINVAL;
		asguard_error("Priv ptr points to NULL\n");
		goto error;
	}

	// mind the off by one counting..
	if (log_idx >= MAX_CONSENSUS_LOG) {
		err = -ENOMEM;
		asguard_error("Log is full\n");
		goto error;
	}

	if (priv->sm_log.commit_idx > log_idx) {
		err = -EPROTO;
		asguard_error("BUG - commit_idx=%d is greater than idx(%d) of entry to commit!\n", priv->sm_log.commit_idx, log_idx);
		goto error;
	}
    // freed by consensus_clean
    entry = kmalloc(sizeof(struct sm_log_entry), GFP_KERNEL);

	if (!entry) {
		err = -ENOMEM;
		goto error;
	}

	entry->cmd = cmd;
	entry->term = term;
	priv->sm_log.entries[log_idx] = entry;

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


