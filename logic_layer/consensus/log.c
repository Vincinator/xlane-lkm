#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <sassy/payload_helper.h>


#include "include/leader.h"
#include <sassy/consensus.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][RSM]"


int apply_log_to_sm(struct state_machine_cmd_log *log, u32 commit_idx)
{
	int err;

	if(!sm_log) {
		err = -EINVAL;
		sassy_error("Log ptr points to NULL\n");
		goto error;
	}

	// measure throughput here! 


	return 0;
error:
	return err;
}


int commit_upto_index(struct state_machine_cmd_log *log, u32 index)
{
	int err;

	if(!sm_log) {
		err = -EINVAL;
		sassy_error("Log ptr points to NULL\n");
		goto error;
	}

	if(index > sm_log.last_idx ){
		err = -EINVAL;
		sassy_error("Given Commit Index is greater than number of entries in log\n");
		goto error;
	}

	if(index < sm_log.commit_idx ){
		err = -EINVAL;
		sassy_error("Already commited to a later index\n");
		goto error;
	}

	err = apply_log_to_sm(index);

	if(err)
		goto error;

	log->commit_idx = index;


	return 0;
error:
	sassy_dbg("Could not commit to Logs. Commit Index %d\n", index);
	return err;
}
EXPORT_SYMBOL(commit_upto_index);


int append_command(struct state_machine_cmd_log *log, struct sm_command *cmd)
{
	int err;
	int last_idx;

	last_idx = sm_log.last_idx;

	if(!sm_log) {
		err = -EINVAL;
		sassy_error("Log ptr points to NULL\n");
		goto error;
	}

	// mind the off by one counting.. last_idx starts at 0
	if(sm_log.max_entries == last_idx + 1){
		err = -ENOMEM;
		sassy_error("Log is full\n");
		goto error;
	}

	if(sm_log.commit_idx > last_idx ){
		err = -EPROTO;
		sassy_error("BUG - commit_idx is greater than last_idx!\n");
		goto error;
	}

	log->entries[last_idx + 1] = cmd;
	log->last_idx = last_idx;


	return 0;
error:
	sassy_dbg("Could not appen command to Logs!\n");
	return err;
}
EXPORT_SYMBOL(append_command);


