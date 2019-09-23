#pragma once

#include <sassy/sassy.h>

#include <sassy/consensus.h>


struct sm_command{
	u32 sm_logvar_id;
	u32 sm_logvar_value;
};

struct sm_log_entry {

	/* Term in which this command was appended to the log
	 */
	u32 term;

	/* The command for the state machine.
	 * Contains the required information to change the state machine.
	 *
	 * A ordered set of commands applied to the state machine will 
	 * transition the state machine to a common state (shared across the cluster).
	 */ 
	struct sm_command cmd;
};


struct sm_log {

	/* Index of the last valid entry in the entries array
	 */
	u32 last_idx;

	/* Index of the last commited entry in the entries array 
	 */
	u32 commit_idx;

	/* Maximum index of the entries array
	 */
	u32 max_entries;

	struct sm_log_entry **entries;

};

int commit_upto_index(struct sm_log *log, u32 index);
int append_command(struct sm_log *log, struct sm_command *cmd);


