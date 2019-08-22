#pragma once

#include <sassy/sassy.h>

struct event {
	
	uint64_t timestamp_tcs;
	
	enum node_state nstate;

	

	enum le_opcode opcode;
	u32 term;
	int target_id; 
}