#pragma once

#include <sassy/sassy.h>

/* MUST not exceed SASSY_PAYLOAD_BYTES in size! */
struct echo_payload {
	u8 protocol_id; /* must be the first element */
	u8 message; /* short message bundled with this hb */
	u8 alive_rp; /* Number of alive processes */
	struct sassy_process_info pinfo[MAX_PROCESSES_PER_HOST];
};
