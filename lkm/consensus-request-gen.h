#pragma once

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/timer.h>

#include "../core/libasraft.h"

#include "../core/ringbuffer.h"
#include "../core/candidate.h"
#include "../core/follower.h"
#include "../core/leader.h"
#include "../core/consensus.h"
#include "../core/module.h"
#include "../core/kvstore.h"
#include "../core/replication.h"


void testcase_X_requests_per_sec(struct consensus_priv *priv, int x);
static enum hrtimer_restart testcase_timer(struct hrtimer *timer);
void testcase_one_shot_big_log(struct consensus_priv *priv);
void testcase_stop_timer(struct consensus_priv *priv);