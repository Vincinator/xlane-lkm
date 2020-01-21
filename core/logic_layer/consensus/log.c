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

//	asguard_dbg("Added %d commands to State Machine.\n", applying);

	return 0;
}

int commit_log(struct consensus_priv *priv)
{
	int err;
	struct state_machine_cmd_log *log = &priv->sm_log;

	err = apply_log_to_sm(priv);

	if (err)
		goto error;

	log->last_applied = log->commit_idx;

	return 0;
error:
	asguard_dbg("Could not commit to Logs. Commit Index %d\n", log->commit_idx);
	return err;
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

	for(i = 0 > priv->sm_log.next_retrans_req_idx ? 0 : priv->sm_log.next_retrans_req_idx; i < priv->sm_log.last_idx; i += priv->max_entries_per_pkt) {
		if(priv->sm_log.next_retrans_req_idx == i)
			continue; // already sent that request

		if(!priv->sm_log.entries[i]){
			priv->sm_log.next_retrans_req_idx = i ;
			break;
		}
		if(i == priv->sm_log.last_idx - 1) {
			priv->sm_log.next_retrans_req_idx = -2;
		}
	}
}
EXPORT_SYMBOL(update_next_retransmission_request_idx);


int append_command(struct state_machine_cmd_log *log, struct sm_command *cmd, s32 term, int log_idx, int unstable)
{
	int err;
	struct sm_log_entry *entry;

	if (!log) {
		err = -EINVAL;
		asguard_error("Log ptr points to NULL\n");
		goto error;
	}

	// mind the off by one counting..
	if (log_idx >= MAX_CONSENSUS_LOG) {
		err = -ENOMEM;
		asguard_error("Log is full\n");
		goto error;
	}

	if (log->commit_idx > log_idx) {
		err = -EPROTO;
		asguard_error("BUG - commit_idx=%d is greater than idx(%d) of entry to commit!\n", log->commit_idx, log_idx);
		goto error;
	}

	entry = kmalloc(sizeof(struct sm_log_entry), GFP_KERNEL);

	if (!entry) {
		err = -ENOMEM;
		goto error;
	}

	entry->cmd = cmd;
	entry->term = term;
	log->entries[log_idx] = entry;

	if (log->last_idx < log_idx)
		log->last_idx = log_idx;


	return 0;
error:
	asguard_dbg("Could not appen command to Logs!\n");
	return err;
}
EXPORT_SYMBOL(append_command);


