#pragma once

#include <sassy/sassy.h>

#define SASSY_NUM_TS_LOG_TYPES 8
#define TIMESTAMP_ARRAY_LIMIT 100000


enum sassy_ts_state {
    SASSY_TS_RUNNING,
    SASSY_TS_READY, 	/* Initialized but not active*/
    SASSY_TS_UNINIT,
    SASSY_TS_LOG_FULL,
};

struct sassy_timestamp_item {
	uint64_t timestamp_tcs;
	//ktime_t correlation_id;
	int target_id; /* Host target id to which the heartbeat was send (if TX) */
};

struct sassy_timestamp_logs {
	struct sassy_timestamp_item *timestamp_items; /* Size is defined with TIMESTAMP_ARRAY_LIMIT macro*/
	int current_timestamps; /* How many timestamps are set in the timestamp */
	struct proc_dir_entry	*proc_dir;
	char *name;
};

struct sassy_stats {
	/* Array of timestamp logs
	 * - Each item corresponds to one timestamping type
	 * - Each item is a pointer to a sassy_timestamp_logs struct
	 */
	struct sassy_timestamp_logs **timestamp_logs;
	int timestamp_amount; /* how many different timestamps types are tracked*/
};

void ts_state_transition_to(struct sassy_device *sdev,
			    enum sassy_ts_state state);

int sassy_ts_stop(struct sassy_device *sdev);
int sassy_ts_start(struct sassy_device *sdev);
int sassy_reset_stats(struct sassy_device *sdev);
