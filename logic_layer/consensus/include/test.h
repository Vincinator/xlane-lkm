#pragma once

#include <sassy/sassy.h>

#include <sassy/consensus.h>

void testcase_stop_timer(struct consensus_priv *priv);
void testcase_one_shot_big_log(struct consensus_priv *priv);
void testcase_X_requests_per_sec(struct consensus_priv *priv, int x);