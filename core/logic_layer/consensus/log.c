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
		asguard_error("BUG - commit_idx is greater than log_idx!\n");
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

	// only update last index if we are not appending missing previous parts
	if (log->last_idx < log_idx)
		log->last_idx++;

	if(!unstable)
		log->stable_idx++; // this is a stable append, so we can increase the idx by 1

	return 0;
error:
	asguard_dbg("Could not appen command to Logs!\n");
	return err;
}
EXPORT_SYMBOL(append_command);


