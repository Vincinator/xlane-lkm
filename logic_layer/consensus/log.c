#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>

#include <linux/slab.h>
#include <linux/kernel.h>

#include "include/leader.h"
#include <asguard/consensus.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][RSM]"


int apply_log_to_sm(struct consensus_priv *priv)
{
	int err;
	struct state_machine_cmd_log *log;
	int applying;

	log = &priv->sm_log;
	applying = log->commit_idx - log->last_applied;


	write_log(&priv->throughput_logger, applying, rdtsc());

//	asguard_dbg("Added %d commands to State Machine. \n", applying);


	return 0;
error:
	return err;
}


int commit_log(struct consensus_priv *priv)
{
	int err;
	struct state_machine_cmd_log *log = &priv->sm_log; 

	err = apply_log_to_sm(priv);

	if(err)
		goto error;

	log->last_applied = log->commit_idx;


	return 0;
error:
	asguard_dbg("Could not commit to Logs. Commit Index %d\n", log->commit_idx);
	return err;
}
EXPORT_SYMBOL(commit_log);


int append_command(struct state_machine_cmd_log *log, struct sm_command *cmd, s32 term)
{
	int err;
	int last_idx;
	struct consensus_priv *priv =
		container_of(log, struct consensus_priv, sm_log);
	struct sm_log_entry *entry;


	last_idx = log->last_idx;

	if(!log) {
		err = -EINVAL;
		asguard_error("Log ptr points to NULL\n");
		goto error;
	}

	// mind the off by one counting.. last_idx starts at 0
	if(MAX_CONSENSUS_LOG <= last_idx + 1){
		err = -ENOMEM;
		asguard_error("Log is full\n");
		goto error;
	}

	if(log->commit_idx > last_idx ){
		err = -EPROTO;
		asguard_error("BUG - commit_idx is greater than last_idx!\n");
		goto error;
	}

	entry = kmalloc(sizeof(struct sm_log_entry), GFP_KERNEL);

	if(!entry){
		asguard_dbg("out of memory!\n");
		err = -ENOMEM;
		goto error;
	}

	entry->cmd = cmd;
	entry->term = term;

	log->entries[last_idx + 1] = entry;
	log->last_idx++; // increase only if it is safe to access the entries array at last_idx! (parallel access)

	return 0;
error:
	asguard_dbg("Could not appen command to Logs!\n");
	return err;
}
EXPORT_SYMBOL(append_command);


