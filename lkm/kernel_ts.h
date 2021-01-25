
#include "../core/libasraft.h"
#include "../core/types.h"




const char *ts_state_string(enum tsstate state);
void ts_state_transition_to(struct asgard_device *sdev,
                            enum tsstate state);

int asgard_write_timestamp(struct asgard_device *sdev,
                           int logid, uint64_t cycles,
                           int target_id);


int asgard_ts_stop(struct asgard_device *sdev);
int asgard_ts_start(struct asgard_device *sdev);
int asgard_reset_stats(struct asgard_device *sdev);
int asgard_clean_timestamping(struct asgard_device *sdev);

int init_timestamping(struct asgard_device *sdev);
