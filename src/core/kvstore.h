#pragma once

#ifdef ASGARD_KERNEL_MODULE


#else
    #include <pthread.h>
    #include <errno.h>
    #include <stdio.h>
    #include <string.h>
    #include "list.h"

#endif

#include "libasraft.h"
#include "logger.h"
#include "consensus.h"
#include "ringbuffer.h"
#include "replication.h"


int consensus_idx_to_buffer_idx(struct state_machine_cmd_log *log, uint32_t dividend);
int append_command(struct consensus_priv *priv, struct data_chunk *dataChunk, int32_t term, int log_idx, int unstable);
void update_stable_idx(struct consensus_priv *priv);
void print_log_state(struct state_machine_cmd_log *log);
int commit_log(struct consensus_priv *priv, int32_t commit_idx);
int32_t get_last_idx_safe(struct consensus_priv *priv);
int32_t get_prev_log_term(struct consensus_priv *cur_priv, int32_t con_idx);