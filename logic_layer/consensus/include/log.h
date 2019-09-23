#pragma once

#include <sassy/sassy.h>

#include <sassy/consensus.h>


struct rsm_command{
	u32 smr_logvar_id;
	u32 smr_logvar_value;
};

struct rsm_log_entry {
	u32 term;
	struct rsm_command rsm_cmd;
};


struct rsm_log {

	u32 last_entry;

	u32 max_entries;

	struct rsm_log_entry *entries;


};
