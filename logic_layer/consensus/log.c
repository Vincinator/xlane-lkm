#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <sassy/payload_helper.h>

#include <linux/slab.h>
#include <linux/kernel.h>

#include "include/leader.h"
#include <sassy/consensus.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][RSM]"


int apply_log_to_sm(struct state_machine_cmd_log *log)
{
	int err;

	if(!log) {
		err = -EINVAL;
		sassy_error("Log ptr points to NULL\n");
		goto error;
	}

	// measure throughput here! 

	sassy_dbg("Applying cmd until commit idx %d\n", log->commit_idx);


	return 0;
error:
	return err;
}


int commit_log(struct consensus_priv *priv)
{
	int err;
	struct state_machine_cmd_log *log = &priv->sm_log; 

	if(!log) {
		err = -EINVAL;
		sassy_error("Log ptr points to NULL\n");
		goto error;
	}

	err = apply_log_to_sm(log);

	if(err)
		goto error;

	log->last_applied = log->commit_idx;


	return 0;
error:
	sassy_dbg("Could not commit to Logs. Commit Index %d\n", index);
	return err;
}
EXPORT_SYMBOL(commit_log);


int append_command(struct state_machine_cmd_log *log, struct sm_command *cmd, int term)
{
	int err;
	int last_idx;
	struct consensus_priv *priv =
		container_of(log, struct consensus_priv, sm_log);
	struct sm_log_entry *entry;


	last_idx = log->last_idx;

	if(!log) {
		err = -EINVAL;
		sassy_error("Log ptr points to NULL\n");
		goto error;
	}

	// mind the off by one counting.. last_idx starts at 0
	if(log->max_entries == last_idx + 1){
		err = -ENOMEM;
		sassy_error("Log is full\n");
		goto error;
	}

	if(log->commit_idx > last_idx ){
		err = -EPROTO;
		sassy_error("BUG - commit_idx is greater than last_idx!\n");
		goto error;
	}

	entry = kmalloc(sizeof(struct sm_log_entry), GFP_KERNEL);

	if(!entry){
		sassy_dbg("out of memory!\n");
		err = -ENOMEM;
		goto error;
	}

	entry->cmd = cmd;
	entry->term = term;

	log->entries[last_idx + 1] = entry;
	log->last_idx = last_idx;

	return 0;
error:
	sassy_dbg("Could not appen command to Logs!\n");
	return err;
}
EXPORT_SYMBOL(append_command);


